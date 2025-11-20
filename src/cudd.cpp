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
#include "basic.hpp"

using namespace fastLEC;

/**
 * CUDD hook function to check global termination flag
 * This function is called by CUDD during reordering and GC operations
 * Returns 0 to terminate the operation, non-zero to continue
 * Note: Hook return value semantics: 0 means stop, non-zero means continue
 */
extern "C" int mycheckhook(DdManager *dd, const char *where, void *f)
{
    // Suppress unused parameter warnings
    (void)dd;
    (void)where;
    (void)f;

    // Check global termination flag
    if (global_solved_for_PPE.load() ||
        fastLEC::ResMgr::get().get_runtime() > fastLEC::Param::get().timeout)
    {
        // Return 0 to terminate the operation (hook semantics: 0 = stop)
        return 0;
    }
    return 1; // Continue the operation
}

/**
 * CUDD termination callback function
 * This function is called by CUDD during node allocation and other frequent
 * operations Returns non-zero (true) to terminate the operation, 0 (false) to
 * continue When this returns true, CUDD will set errorCode to CUDD_TERMINATION
 */
extern "C" int myterminationcallback(const void *arg)
{
    // Suppress unused parameter warnings
    (void)arg;

    // Check global termination flag
    if (global_solved_for_PPE.load() ||
        fastLEC::ResMgr::get().get_runtime() > fastLEC::Param::get().timeout)
    {
        // Return 1 (true) to terminate the operation
        return 1;
    }
    return 0; // Continue the operation
}

/**
 * Print a dd summary using BDD wrapper
 * pr = 0 : prints nothing
 * pr = 1 : prints counts of nodes and minterms
 * pr = 2 : prints counts + disjoint sum of product
 * pr = 3 : prints counts + list of nodes
 * pr > 3 : prints counts + disjoint sum of product + list of nodes
 * @param the BDD node
 */
void print_dd(std::shared_ptr<fastLEC::CuddManager> manager,
              const class fastLEC::CuddBDD &bdd,
              int n,
              int pr)
{
    BDDUtils::printDD(manager, bdd, n, pr);
}

/**
 * Writes a dot file representing the argument BDDs
 * @param the BDD object
 */
void write_dd(std::shared_ptr<fastLEC::CuddManager> manager,
              const class fastLEC::CuddBDD &bdd,
              const char *filename)
{
    BDDUtils::writeDD(manager, bdd, filename);
}

fastLEC::ret_vals
fastLEC::Prover::seq_BDD_cudd(std::shared_ptr<fastLEC::XAG> xag)
{
    double start_time = fastLEC::ResMgr::get().get_runtime();

    if (xag == nullptr)
    {
        printf("c [BDD] Error: XAG not found, returning UNKNOWN\n");
        return ret_vals::ret_UNK;
    }

    // Declare variables outside try block so they can be used in catch and after try-catch
    std::shared_ptr<CuddManager> manager = nullptr;
    ret_vals ret = ret_vals::ret_UNK;
    int gate_count = 0;
    int total_gates = xag->used_gates.size();
    bool terminated_by_other = false;
    bool terminated_by_timeout = false;

    try
    {
        // Create CUDD manager with shared_ptr for automatic cleanup
        manager = std::make_shared<CuddManager>(
            0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);

        // Add global termination hook and callback for immediate response
        // Termination callback is checked more frequently (during node
        // allocation, etc.)
        manager->registerTerminationCallback();
        // Hook is checked during reordering and GC
        manager->addGlobalTerminationHook();

        // Create BDD nodes vector with smart pointer management
        std::vector<class fastLEC::CuddBDD> nodes(xag->max_var + 1);
        nodes[0] = BDDFactory::createZero(manager);

        // Create input variables
        for (int i : xag->PI)
        {
            int ivar = aiger_var(i);
            nodes[ivar] = BDDFactory::createVar(manager);
        }

        // Process gates with hook-based termination checking

        for (int gid : xag->used_gates)
        {
            // Check if CUDD detected termination via hook/callback
            if (manager->hasTermination())
            {
                break;
            }

            Gate &g = xag->gates[gid];

            class fastLEC::CuddBDD i1 = nodes[aiger_var(g.inputs[0])];
            if (aiger_sign(g.inputs[0]))
                i1 = !i1;

            class fastLEC::CuddBDD i2 = nodes[aiger_var(g.inputs[1])];
            if (aiger_sign(g.inputs[1]))
                i2 = !i2;

            if (g.type == GateType::AND2)
            {
                nodes[aiger_var(g.output)] = i1 & i2;
                // Check if operation failed due to termination
                if (nodes[aiger_var(g.output)].isNull() &&
                    manager->hasTermination())
                {
                    break;
                }
            }
            else if (g.type == GateType::XOR2)
            {
                nodes[aiger_var(g.output)] = i1 ^ i2;
                // Check if operation failed due to termination
                if (nodes[aiger_var(g.output)].isNull() &&
                    manager->hasTermination())
                {
                    break;
                }
            }
            gate_count++;
        }

        // Final check: timeout or terminated by other thread
        double final_time = fastLEC::ResMgr::get().get_runtime();
        bool actual_timeout = final_time > Param::get().timeout;

        if (manager->hasTermination())
        {
            if (actual_timeout)
            {
                terminated_by_timeout = true;
            }
            else if (global_solved_for_PPE.load())
            {
                terminated_by_other = true;
            }
            ret = ret_vals::ret_UNK;
        }
        else
        {
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

    }
    catch (const std::exception &e)
    {
        // Even on exception, try to output basic statistics if manager is
        // available
        printf("c [BDD] Error: %s, returning UNKNOWN\n", e.what());
        ret = ret_vals::ret_UNK;
    }

    // Clean up the hook and callback
    if (manager != nullptr)
    {
        manager->removeGlobalTerminationHook();
        manager->unregisterTerminationCallback();
    }
    
    // Always output statistics, regardless of termination reason
    const char *timeout_status = terminated_by_timeout ? " (TO)"
        : terminated_by_other                          ? " (ENG)"
        : (manager != nullptr && manager->hasTermination()) ? " (UNK)"
                                                             : " ";

    if (manager != nullptr)
    {
        printf("c [BDD] result = %d, [nodes = %ld, vars = %d, reorderings = "
                "%d, memory = %ld bytes] [time = %f] [processed %d/%d gates], "
                "%s \n",
                ret,
                manager->readNodeCount(),
                manager->readSize(),
                manager->readReorderings(),
                (long)manager->readMemoryInUse(),
                fastLEC::ResMgr::get().get_runtime() - start_time,
                gate_count,
                total_gates,
                timeout_status);
    }
    else
    {
        printf("c [BDD] result = %d, [manager not initialized] [time = %f] "
                "[processed %d/%d gates]%s \n",
                ret,
                fastLEC::ResMgr::get().get_runtime() - start_time,
                gate_count,
                total_gates,
                timeout_status);
    }
    fflush(stdout);

    return ret;
    
}
