#include "fastLEC.hpp"
#include "parser.hpp"
#include "basic.hpp"
#include <mutex>
#include <thread>
#include <atomic>
#include <algorithm>

#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

extern "C"
{
#include "../deps/sylvan/src/sylvan.h"
#include "../deps/sylvan/src/sylvan_obj.hpp"
#include "../deps/sylvan/src/sylvan_cache.h"
}

using namespace fastLEC;
using namespace sylvan;

namespace
{
std::shared_ptr<fastLEC::XAG> g_current_xag;
std::mutex g_xag_mutex;

// Sylvan quit control - prevent multiple calls to sylvan_quit()
std::atomic<bool> g_sylvan_finished{false};
std::mutex g_timeout_mutex;

// Sylvan size configuration (calculated based on system resources)
long long g_nodes_initial = 1LL << 22;
long long g_nodes_max = 1LL << 26;
long long g_cache_initial = 1LL << 22;
long long g_cache_max = 1LL << 26;
} // namespace

/**
 * Get total system memory in bytes
 */
size_t get_system_memory()
{
#ifdef __APPLE__
    uint64_t mem_size = 0;
    size_t len = sizeof(mem_size);
    if (sysctlbyname("hw.memsize", &mem_size, &len, nullptr, 0) == 0)
    {
        return static_cast<size_t>(mem_size);
    }
    return 8ULL * 1024 * 1024 * 1024; // Default 8GB if can't detect
#elif defined(__linux__)
    struct sysinfo info;
    if (sysinfo(&info) == 0)
    {
        return static_cast<size_t>(info.totalram) * info.mem_unit;
    }
    return 8ULL * 1024 * 1024 * 1024; // Default 8GB if can't detect
#elif defined(_WIN32)
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
    {
        return static_cast<size_t>(status.ullTotalPhys);
    }
    return 8ULL * 1024 * 1024 * 1024; // Default 8GB if can't detect
#else
    // Unknown platform, use conservative default
    return 8ULL * 1024 * 1024 * 1024; // Default 8GB
#endif
}

/**
 * Calculate optimal sylvan sizes based on system resources
 * Memory usage: 24 bytes per node, 36 bytes per cache bucket
 *
 * @param n_workers Number of worker threads
 * @param n_cores Number of CPU cores
 * @param total_memory Total system memory in bytes
 */
void calculate_sylvan_sizes(int n_workers, int n_cores, size_t total_memory)
{
    if (n_workers <= 0 || n_cores <= 0) n_workers = n_cores = 1; // avoid div0
    // 计算允许的最大内存，总不超过 n_workers/n_cores * total
    double parallel_ratio = static_cast<double>(n_workers) / n_cores;
    double max_available_memory = std::min(total_memory * parallel_ratio, static_cast<double>(total_memory));
    // 留出10%冗余给系统
    size_t target_memory = static_cast<size_t>(max_available_memory * 0.9);

    // 50%给nodes, 50%给cache
    size_t nodes_memory = target_memory / 2;
    size_t cache_memory = target_memory - nodes_memory;

    // 计算能拿到多少node和cache
    long long nodes_max_calc = static_cast<long long>(nodes_memory / 24);
    long long cache_max_calc = static_cast<long long>(cache_memory / 36);

    // 转为2的指数（向下取整到 2^n）
    auto floor_pow2 = [](long long x) -> long long {
        if (x < 1) return 1;
        long long res = 1;
        while (res <= x/2) res <<= 1;
        return res;
    };

    nodes_max_calc = floor_pow2(nodes_max_calc);
    cache_max_calc = floor_pow2(cache_max_calc);

    // 初始为最大的1/8, 但至少2^20
    auto initial_of_max = [](long long maxv) -> long long {
        long long init = maxv >> 3;
        if (init < (1LL << 20)) return 1LL << 20;
        // 转换为2的幂
        long long res = 1;
        while (res <= init/2) res <<= 1;
        return res;
    };

    long long nodes_initial_calc = std::min(initial_of_max(nodes_max_calc), nodes_max_calc);
    long long cache_initial_calc = std::min(initial_of_max(cache_max_calc), cache_max_calc);

    // 合理限制上下界
    const long long min_entry = 1LL << 20, max_entry = 1LL << 32; // practical upper bound
    nodes_max_calc = std::max(min_entry, std::min(max_entry, nodes_max_calc));
    cache_max_calc = std::max(min_entry, std::min(max_entry, cache_max_calc));
    nodes_initial_calc = std::max(min_entry, std::min(nodes_max_calc, nodes_initial_calc));
    cache_initial_calc = std::max(min_entry, std::min(cache_max_calc, cache_initial_calc));

    g_nodes_initial = nodes_initial_calc;
    g_nodes_max = nodes_max_calc;
    g_cache_initial = cache_initial_calc;
    g_cache_max = cache_max_calc;

    // 实际内存消耗
    double nodes_mem_gb = g_nodes_max * 24.0 / (1024 * 1024 * 1024);
    double cache_mem_gb = g_cache_max * 36.0 / (1024 * 1024 * 1024);
    double total_mem_gb = nodes_mem_gb + cache_mem_gb;

    if (fastLEC::Param::get().verbose > 0)
    {
        printf("c [Sylvan] Configuration: workers=%d, cores=%d, "
               "total_memory=%.2f GB, target_memory=%.2f GB\n",
               n_workers, n_cores,
               total_memory / (1024.0 * 1024 * 1024),
               target_memory / (1024.0 * 1024 * 1024));
        printf("c [Sylvan] Nodes: initial=2^%d (%.2f MB), max=2^%d (%.2f GB)\n",
               (int)std::log2(g_nodes_initial), g_nodes_initial*24.0/(1024*1024),
               (int)std::log2(g_nodes_max), g_nodes_max*24.0/(1024*1024*1024));
        printf("c [Sylvan] Cache: initial=2^%d (%.2f MB), max=2^%d (%.2f GB)\n",
               (int)std::log2(g_cache_initial), g_cache_initial*36.0/(1024*1024),
               (int)std::log2(g_cache_max), g_cache_max*36.0/(1024*1024*1024));
        printf("c [Sylvan] Total allocated memory: %.2f GB (limit: %.2f GB, ratio=%.2f)\n",
               total_mem_gb, max_available_memory/(1024.0*1024*1024), parallel_ratio);
        fflush(stdout);
    }
}

void set_current_xag(std::shared_ptr<fastLEC::XAG> xag)
{
    std::lock_guard<std::mutex> lock(g_xag_mutex);
    g_current_xag = xag;
}

std::shared_ptr<fastLEC::XAG> get_current_xag()
{
    std::lock_guard<std::mutex> lock(g_xag_mutex);
    return g_current_xag;
}

void clear_current_xag()
{
    std::lock_guard<std::mutex> lock(g_xag_mutex);
    g_current_xag.reset();
}

bool is_timeout_reached()
{
    return fastLEC::ResMgr::get().get_runtime() > fastLEC::Param::get().timeout;
}

// Safe wrapper for sylvan_quit() - ensures it's only called once
void safe_sylvan_quit()
{
    std::lock_guard<std::mutex> lock(g_timeout_mutex);
    if (!g_sylvan_finished.load())
    {
        g_sylvan_finished.store(true);
        sylvan_quit();
    }
}

TASK_0(fastLEC::ret_vals, miter_build_bdd)
{
    auto xag = get_current_xag();
    if (!xag)
        return fastLEC::ret_vals::ret_UNK;

    // Check timeout at task start
    if (is_timeout_reached())
        return ret_vals::ret_UNK;

    std::vector<Bdd> vars(xag->max_var + 1);
    vars[0] = Bdd::bddZero();

    for (int pi : xag->PI)
    {
        int vpi = aiger_var(pi);
        vars[vpi] = Bdd::bddVar(vpi);
    }

    for (int gid : xag->used_gates)
    {
        Gate &g = xag->gates[gid];
        int vout = aiger_var(g.output);
        int v1 = aiger_var(g.inputs[0]);
        int v2 = aiger_var(g.inputs[1]);
        // Check timeout before BDD operation
        if (is_timeout_reached())
            return ret_vals::ret_UNK;

        Bdd b1 = aiger_sign(g.inputs[0]) ? !vars[v1] : vars[v1];
        Bdd b2 = aiger_sign(g.inputs[1]) ? !vars[v2] : vars[v2];
        if (g.type == GateType::AND2)
            vars[vout] = b1 * b2;
        else if (g.type == GateType::XOR2)
            vars[vout] = b1 ^ b2;
        else
            assert(false);

        // Check timeout after BDD operation
        if (is_timeout_reached())
            return ret_vals::ret_UNK;
    }

    // Check timeout before result check
    YIELD_NEWFRAME(); // Let Lace check if interruption is needed
    if (is_timeout_reached())
        return ret_vals::ret_UNK;

    int vpo = aiger_var(xag->PO);

    if (aiger_sign(xag->PO))
    {
        if (vars[vpo] == Bdd::bddOne())
            return ret_vals::ret_UNS;
        else
            return ret_vals::ret_SAT;
    }
    else
    {
        if (vars[vpo] == Bdd::bddZero())
            return ret_vals::ret_UNS;
        else
            return ret_vals::ret_SAT;
    }

    return fastLEC::ret_vals::ret_UNK;
}

TASK_0(fastLEC::ret_vals, _main)
{

    // Initialize Sylvan
    // With starting size of the nodes table 1 << 21, and maximum size 1 << 27.
    // With starting size of the cache table 1 << 20, and maximum size 1 << 20.
    // Memory usage: 24 bytes per node, and 36 bytes per cache bucket
    // - 1<<24 nodes: 384 MB
    // - 1<<25 nodes: 768 MB
    // - 1<<26 nodes: 1536 MB
    // - 1<<27 nodes: 3072 MB
    // - 1<<24 cache: 576 MB
    // - 1<<25 cache: 1152 MB
    // - 1<<26 cache: 2304 MB
    // - 1<<27 cache: 4608 MB
    // - 1<<28 cache: 9216 MB
    // - 1<<29 cache: 18 GB
    // - 1<<30 cache: 36 GB
    // - 1<<31 cache: 72 GB
    // - 1<<32 cache: 144 GB

    sylvan_set_sizes(
        g_nodes_initial, g_nodes_max, g_cache_initial, g_cache_max);
    sylvan_init_package();

    // Initialize the BDD module with granularity 1 (cache every operation)
    // A higher granularity (e.g. 6) often results in better performance in
    // practice
    sylvan_init_bdd();

    fastLEC::ret_vals ret = CALL(miter_build_bdd);

    // Get Sylvan statistics
    size_t nodes_filled = 0, nodes_total = 0;
    sylvan_table_usage(&nodes_filled, &nodes_total);
    size_t cache_used = cache_getused();
    size_t cache_size = cache_getsize();

    // Calculate memory usage and convert to MB
    size_t total_memory_bytes = 24ULL * nodes_total + 36ULL * cache_size;
    double total_memory_mb = total_memory_bytes / (1024.0 * 1024.0);

    printf("c [pBDD] result = %d, workers = %d, "
           "[nodes = %zu/%zu, cache = %zu/%zu, memory = %.2f MB]\n",
           ret,
           lace_workers(),
           nodes_filled,
           nodes_total,
           cache_used,
           cache_size,
           total_memory_mb);
    fflush(stdout);

    // Don't call sylvan_quit() here - lace workers may still be running
    // Let the main thread handle cleanup after lace workers are stopped
    // safe_sylvan_quit() will be called in para_BDD_sylvan after lace_stop()

    return ret;
}

fastLEC::ret_vals
fastLEC::Prover::para_BDD_sylvan(std::shared_ptr<fastLEC::XAG> xag, int n_t)
{
    // Reset sylvan quit flag at the start of each call
    g_sylvan_finished.store(false);

    set_current_xag(xag);

    std::thread timeout_thread = std::thread(
        [&]()
        {
            while (!g_sylvan_finished.load())
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(50)); // Check every 50ms

                // Use fastLEC's runtime check
                if (fastLEC::ResMgr::get().get_runtime() >
                    fastLEC::Param::get().timeout)
                {
                    printf("c [Sylvan] Timeout reached\n");
                    fflush(stdout);

                    safe_sylvan_quit();
                    break;
                }
            }
        });

    int n_workers = n_t;

    // Calculate optimal sylvan sizes based on system resources
    int n_cores = static_cast<int>(std::thread::hardware_concurrency());
    size_t total_memory = get_system_memory();
    calculate_sylvan_sizes(n_workers, n_cores, total_memory);

    size_t deque_size = 0;

    lace_start(n_workers, deque_size);

    fastLEC::ret_vals ret = ret_UNK;
    try
    {
        ret = RUN(_main);
        g_sylvan_finished.store(true);
        // std::cout << *g_current_xag << std::endl;
    }
    catch (...)
    {
        printf("c [Sylvan] Exception occurred during BDD computation\n");
        ret = ret_UNK;
    }

    timeout_thread.join();

    printf("c [Sylvan] Quitting Sylvan\n");
    fflush(stdout);

    safe_sylvan_quit();
    lace_stop();

    return ret;
}