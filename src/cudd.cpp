#include <cstring>
#include <sys/stat.h>
#include <cstdio>
#include <cassert>


extern "C"
{
#include "../deps/cudd/config.h"
#include "../deps/cudd/cudd/cudd.h"
}

// Undefine the fail macro to avoid conflicts with C++ standard library
#ifdef fail
#undef fail
#endif

#include "fastLEC.hpp"
#include "cudd.hpp"
#include "parser.hpp"

using namespace fastLEC;


/**
 * Print a dd summary using BDD wrapper
 * pr = 0 : prints nothing
 * pr = 1 : prints counts of nodes and minterms
 * pr = 2 : prints counts + disjoint sum of product
 * pr = 3 : prints counts + list of nodes
 * pr > 3 : prints counts + disjoint sum of product + list of nodes
 * @param the BDD node
 */
void print_dd(std::shared_ptr<fastLEC::CuddManager> manager, const class fastLEC::BDD& bdd, int n, int pr)
{
    BDDUtils::printDD(manager, bdd, n, pr);
}

/**
 * Writes a dot file representing the argument BDDs
 * @param the BDD object
 */
void write_dd(std::shared_ptr<fastLEC::CuddManager> manager, const class fastLEC::BDD& bdd, const char* filename)
{
    BDDUtils::writeDD(manager, bdd, filename);
}

fastLEC::ret_vals fastLEC::Prover::seq_BDD_cudd(std::shared_ptr<fastLEC::XAG> xag)
{
    double start_time = fastLEC::ResMgr::get().get_runtime();

    if (xag == nullptr)
    {
        printf("c [BDD] Error: XAG not found, returning UNKNOWN\n");
        return ret_vals::ret_UNK;
    }

    try
    {
        // Create CUDD manager with shared_ptr for automatic cleanup
        auto manager = std::make_shared<CuddManager>(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
        
        // Set timeout based on remaining time (using a default timeout of 3600 seconds)
        double remaining_time = fastLEC::Param::get().timeout - fastLEC::ResMgr::get().get_runtime();
        if (remaining_time > 0) {
            // Convert to milliseconds and set timeout
            unsigned long timeout_ms = static_cast<unsigned long>(remaining_time * 1000);
            manager->setTimeLimit(timeout_ms);
            // Reset start time to current time for proper timeout calculation
            manager->resetStartTime();
        }
        
        // Create BDD nodes vector with smart pointer management
        std::vector<class fastLEC::BDD> nodes(xag->max_var + 1);
        nodes[0] = BDDFactory::createZero(manager);

        // Create input variables
        for (int i : xag->PI)
        {
            int ivar = aiger_var(i);
            nodes[ivar] = BDDFactory::createVar(manager);
        }

        // Process gates
        bool timeout_detected = false;
        for (int gid : xag->used_gates)
        {
            // Check for timeout during processing
            if (manager->hasTimeout() || manager->hasTermination()) {
                timeout_detected = true;
                break;
            }
            
            // Check remaining time (using a default timeout of 3600 seconds)
            if (fastLEC::ResMgr::get().get_runtime() > 3600.0) {
                timeout_detected = true;
                break;
            }
            
            Gate &g = xag->gates[gid];

            class fastLEC::BDD i1 = nodes[aiger_var(g.inputs[0])];
            if (aiger_sign(g.inputs[0]))
                i1 = !i1;

            class fastLEC::BDD i2 = nodes[aiger_var(g.inputs[1])];
            if (aiger_sign(g.inputs[1]))
                i2 = !i2;

            if (g.type == GateType::AND2)
            {
                nodes[aiger_var(g.output)] = i1 & i2;
            }
            else if (g.type == GateType::XOR2)
            {
                nodes[aiger_var(g.output)] = i1 ^ i2;
            }
            
            // Check if the operation resulted in timeout (empty BDD)
            if (nodes[aiger_var(g.output)].isNull()) {
                timeout_detected = true;
                break;
            }
        }

        ret_vals ret = ret_vals::ret_UNK;

        // Check if timeout was detected during processing
        if (timeout_detected || manager->hasTimeout() || manager->hasTermination()) {
            ret = ret_vals::ret_UNK;
        } else {
            // Normal result evaluation
            int po_var = aiger_var(xag->PO);
            if (po_var < 0 || po_var > xag->max_var || nodes[po_var].isNull())
            {
                ret = ret_vals::ret_UNK;
            }
            else
            {
                if (aiger_sign(xag->PO))
                {
                    if (nodes[po_var] == !BDDFactory::createZero(manager))
                        ret = ret_vals::ret_UNS;
                    else
                        ret = ret_vals::ret_SAT;
                }
                else
                {
                    if (nodes[po_var] == BDDFactory::createZero(manager))
                        ret = ret_vals::ret_UNS;
                    else
                        ret = ret_vals::ret_SAT;
                }
            }
        }

        // Always output statistics, regardless of timeout
        bool timed_out = timeout_detected || manager->hasTimeout() || manager->hasTermination();
        const char* timeout_status = timed_out ? " (TO)" : "";
        
        printf("c [BDD] result = %d, [nodes = %ld, vars = %d, reorderings = %d, memory = %ld bytes] [time = %f]%s \n", 
            ret, 
            manager->readNodeCount(), 
            manager->readSize(),
            manager->readReorderings(),
            (long)manager->readMemoryInUse(), 
            fastLEC::ResMgr::get().get_runtime() - start_time,
            timeout_status);
        fflush(stdout);

        return ret;
    }
    catch (const std::exception& e)
    {
        // Even on exception, try to output basic statistics if manager is available
        printf("c [BDD] Error: %s, returning UNKNOWN\n", e.what());
        return ret_vals::ret_UNK;
    }
}
