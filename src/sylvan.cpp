#include "fastLEC.hpp"
#include "parser.hpp"
#include "basic.hpp"
#include <mutex>

extern "C" {
#include "../deps/sylvan/src/sylvan.h"
#include "../deps/sylvan/src/sylvan_obj.hpp"
#include "../deps/sylvan/src/sylvan_cache.h"
}

using namespace fastLEC;
using namespace sylvan;

namespace {
    std::shared_ptr<fastLEC::XAG> g_current_xag;
    std::mutex g_xag_mutex;
}

void set_current_xag(std::shared_ptr<fastLEC::XAG> xag) {
    std::lock_guard<std::mutex> lock(g_xag_mutex);
    g_current_xag = xag;
}

std::shared_ptr<fastLEC::XAG> get_current_xag() {
    std::lock_guard<std::mutex> lock(g_xag_mutex);
    return g_current_xag;
}

void clear_current_xag() {
    std::lock_guard<std::mutex> lock(g_xag_mutex);
    g_current_xag.reset();
}

TASK_0(fastLEC::ret_vals, miter_build_bdd)
{
    auto xag = get_current_xag();
    if (!xag) {
        printf("c [Sylvan] Error: No XAG object set\n");
        return fastLEC::ret_vals::ret_UNK;
    }

    std::vector<Bdd> vars(xag->max_var+1);
    for(int pi : xag->PI){
        int vpi = aiger_var(pi) - 1;
        vars[vpi] = Bdd::bddVar(vpi);
    }

    int cnt = 0;
    for (int gid : xag->used_gates){
        Gate &g = xag->gates[gid];
        int vout = aiger_var(g.output) - 1;
        int v1 = aiger_var(g.inputs[0]) - 1;
        int v2 = aiger_var(g.inputs[1]) - 1;
        Bdd b1 = aiger_sign(g.inputs[0]) ? !vars[v1] : vars[v1];
        Bdd b2 = aiger_sign(g.inputs[1]) ? !vars[v2] : vars[v2];
        if(g.type == GateType::AND2){
            vars[vout] = b1 * b2;
        }
        else if (g.type == GateType::XOR2){
            vars[vout] = b1 ^ b2;
        }else{
            assert(false);
        }
        if(cnt++ % 10 == 9){
            if(fastLEC::ResMgr::get().get_runtime() > fastLEC::Param::get().timeout) {
                return ret_vals::ret_UNK;
            }
        }
    }
    
    int vpo = aiger_var(xag->PO) - 1;

    if(aiger_sign(xag->PO)){
        if(vars[vpo] == Bdd::bddOne()){
            return ret_vals::ret_UNS;
        } else {
            return ret_vals::ret_SAT;
        }
    } else {
        if(vars[vpo] == Bdd::bddZero()){
            return ret_vals::ret_UNS;
        } else {
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
    sylvan_set_sizes(1LL<<22, 1LL<<26, 1LL<<22, 1LL<<26);
    sylvan_init_package();

    // Initialize the BDD module with granularity 1 (cache every operation)
    // A higher granularity (e.g. 6) often results in better performance in practice
    sylvan_init_bdd();
    
    fastLEC::ret_vals ret = CALL(miter_build_bdd);

    // 获取 Sylvan 统计信息
    size_t nodes_filled = 0, nodes_total = 0;
    sylvan_table_usage(&nodes_filled, &nodes_total);
    size_t cache_used = cache_getused();
    size_t cache_size = cache_getsize();
    
    // 计算内存使用量并转换为 MB
    size_t total_memory_bytes = 24ULL * nodes_total + 36ULL * cache_size;
    double total_memory_mb = total_memory_bytes / (1024.0 * 1024.0);
    
    printf("c [pBDD] result = %d, "
        "[nodes = %zu/%zu, cache = %zu/%zu, memory = %.2f MB]\n", 
        ret, nodes_filled, nodes_total, cache_used, cache_size, total_memory_mb);
    fflush(stdout);

    // Report statistics (if SYLVAN_STATS is 1 in the configuration)
    // sylvan_stats_report(stdout);

    // And quit, freeing memory
    sylvan_quit();

    return ret;
}

fastLEC::ret_vals fastLEC::Prover::para_BDD_sylvan(std::shared_ptr<fastLEC::XAG> xag, int n_t)
{
    set_current_xag(xag);
    
    int n_workers = n_t;
    size_t deque_size = 0;

    lace_start(n_workers, deque_size);

    fastLEC::ret_vals ret = ret_UNK;
    ret = RUN(_main);
    
    clear_current_xag();

    return ret;
}