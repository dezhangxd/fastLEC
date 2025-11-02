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

// Force abort control
std::atomic<bool> g_force_abort{false};
std::thread g_timeout_thread;
std::atomic<bool> g_timeout_thread_running{false};

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
    // Use at most 70% of total memory for sylvan
    // Reserve 30% for OS and other processes
    size_t available_memory = static_cast<size_t>(total_memory * 0.7);

    // Memory per byte: 24 bytes per node, 36 bytes per cache bucket
    // Typical ratio: cache should be 1.5-2x larger than nodes for good
    // performance Let's allocate: 40% for nodes, 60% for cache
    size_t nodes_memory = static_cast<size_t>(available_memory * 0.4);
    size_t cache_memory = static_cast<size_t>(available_memory * 0.6);

    // Calculate maximum sizes (in number of entries)
    // nodes_max = nodes_memory / 24
    // cache_max = cache_memory / 36
    long long nodes_max_calc = static_cast<long long>(nodes_memory / 24);
    long long cache_max_calc = static_cast<long long>(cache_memory / 36);

    // Convert to power of 2 (find the largest 2^n that fits)
    auto log2_floor = [](long long x) -> int
    {
        if (x <= 0)
            return 0;
        int log = 0;
        while (x > 1)
        {
            x >>= 1;
            log++;
        }
        return log;
    };

    // Find appropriate 2^n sizes
    int nodes_max_exp = log2_floor(nodes_max_calc);
    int cache_max_exp = log2_floor(cache_max_calc);

    // Set reasonable bounds:
    // Minimum: 1<<20 (small systems)
    // Maximum: 1<<32 (very large systems, but practical max is 1<<30)
    nodes_max_exp = std::max(20, std::min(30, nodes_max_exp));
    cache_max_exp = std::max(20, std::min(30, cache_max_exp));

    // Initial size should be smaller (1/4 to 1/16 of max)
    // But at least 1<<20 for performance
    int nodes_initial_exp = std::max(20, nodes_max_exp - 4);
    int cache_initial_exp = std::max(20, cache_max_exp - 4);

    // Adjust based on number of workers
    // More workers may need slightly larger initial size
    if (n_workers > 8)
    {
        nodes_initial_exp = std::min(nodes_initial_exp + 1, nodes_max_exp);
        cache_initial_exp = std::min(cache_initial_exp + 1, cache_max_exp);
    }

    g_nodes_initial = 1LL << nodes_initial_exp;
    g_nodes_max = 1LL << nodes_max_exp;
    g_cache_initial = 1LL << cache_initial_exp;
    g_cache_max = 1LL << cache_max_exp;

    // Calculate actual memory usage
    size_t nodes_mem = 24ULL * g_nodes_max;
    size_t cache_mem = 36ULL * g_cache_max;
    double total_mem_gb = (nodes_mem + cache_mem) / (1024.0 * 1024.0 * 1024.0);

    if (fastLEC::Param::get().verbose > 0)
    {
        printf("c [Sylvan] Configuration: workers=%d, cores=%d, "
               "total_memory=%.2f GB\n",
               n_workers,
               n_cores,
               total_memory / (1024.0 * 1024.0 * 1024.0));
        printf("c [Sylvan] Nodes: initial=2^%d (%.2f MB), max=2^%d (%.2f GB)\n",
               nodes_initial_exp,
               g_nodes_initial * 24.0 / (1024 * 1024),
               nodes_max_exp,
               g_nodes_max * 24.0 / (1024 * 1024 * 1024));
        printf("c [Sylvan] Cache: initial=2^%d (%.2f MB), max=2^%d (%.2f GB)\n",
               cache_initial_exp,
               g_cache_initial * 36.0 / (1024 * 1024),
               cache_max_exp,
               g_cache_max * 36.0 / (1024 * 1024 * 1024));
        printf("c [Sylvan] Total allocated memory: %.2f GB\n", total_mem_gb);
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

// Check if timeout is reached using fastLEC's time functions
bool is_timeout_reached()
{
    // Check force abort flag
    if (g_force_abort.load())
    {
        return true;
    }

    // Use fastLEC's runtime check
    return fastLEC::ResMgr::get().get_runtime() > fastLEC::Param::get().timeout;
}

// Start force timeout control
void start_force_timeout_control()
{
    if (g_timeout_thread_running.load())
    {
        return;
    }

    g_force_abort.store(false);
    g_timeout_thread_running.store(true);

    g_timeout_thread = std::thread(
        [&]()
        {
            while (g_timeout_thread_running.load())
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(50)); // Check every 50ms

                // Use fastLEC's runtime check
                if (fastLEC::ResMgr::get().get_runtime() >
                    fastLEC::Param::get().timeout)
                {
                    g_force_abort.store(true);
                    break;
                }
            }
        });
}

// Stop force timeout control
void stop_force_timeout_control()
{
    if (!g_timeout_thread_running.load())
    {
        return;
    }

    g_timeout_thread_running.store(false);
    if (g_timeout_thread.joinable())
    {
        g_timeout_thread.join();
    }
    g_force_abort.store(false);
}

TASK_0(fastLEC::ret_vals, miter_build_bdd)
{
    auto xag = get_current_xag();
    if (!xag)
    {
        return fastLEC::ret_vals::ret_UNK;
    }

    // Check timeout at task start
    if (is_timeout_reached())
    {
        return ret_vals::ret_UNK;
    }

    std::vector<Bdd> vars(xag->max_var + 1);
    for (int pi : xag->PI)
    {
        int vpi = aiger_var(pi) - 1;
        vars[vpi] = Bdd::bddVar(vpi);
    }

    int cnt = 0;
    for (int gid : xag->used_gates)
    {
        Gate &g = xag->gates[gid];
        int vout = aiger_var(g.output) - 1;
        int v1 = aiger_var(g.inputs[0]) - 1;
        int v2 = aiger_var(g.inputs[1]) - 1;
        // Check timeout before BDD operation
        if (is_timeout_reached())
        {
            return ret_vals::ret_UNK;
        }

        Bdd b1 = aiger_sign(g.inputs[0]) ? !vars[v1] : vars[v1];
        Bdd b2 = aiger_sign(g.inputs[1]) ? !vars[v2] : vars[v2];
        if (g.type == GateType::AND2)
        {
            vars[vout] = b1 * b2;
        }
        else if (g.type == GateType::XOR2)
        {
            vars[vout] = b1 ^ b2;
        }
        else
        {
            assert(false);
        }

        // Check timeout after BDD operation
        if (is_timeout_reached())
        {
            return ret_vals::ret_UNK;
        }
        // More frequent timeout checks
        if (cnt++ % 3 == 2)
        { // Check every 3 gates for more frequent checks
            if (is_timeout_reached())
            {
                return ret_vals::ret_UNK;
            }
        }
    }

    // Check timeout before result check
    YIELD_NEWFRAME(); // Let Lace check if interruption is needed
    if (is_timeout_reached())
    {
        return ret_vals::ret_UNK;
    }

    int vpo = aiger_var(xag->PO) - 1;

    if (aiger_sign(xag->PO))
    {
        if (vars[vpo] == Bdd::bddOne())
        {
            return ret_vals::ret_UNS;
        }
        else
        {
            return ret_vals::ret_SAT;
        }
    }
    else
    {
        if (vars[vpo] == Bdd::bddZero())
        {
            return ret_vals::ret_UNS;
        }
        else
        {
            return ret_vals::ret_SAT;
        }
    }

    return fastLEC::ret_vals::ret_UNS;
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

    // And quit, freeing memory
    sylvan_quit();

    return ret;
}

fastLEC::ret_vals
fastLEC::Prover::para_BDD_sylvan(std::shared_ptr<fastLEC::XAG> xag, int n_t)
{
    set_current_xag(xag);

    // Start force timeout control
    start_force_timeout_control();

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
    }
    catch (...)
    {
        printf("c [Sylvan] Exception occurred during BDD computation\n");
        ret = ret_UNK;
    }

    // Stop force timeout control
    stop_force_timeout_control();

    clear_current_xag();

    return ret;
}