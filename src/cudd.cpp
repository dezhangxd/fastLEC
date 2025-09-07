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
void print_dd(std::shared_ptr<CuddManager> manager, const BDD& bdd, int n, int pr)
{
    BDDUtils::printDD(manager, bdd, n, pr);
}

/**
 * Writes a dot file representing the argument BDDs
 * @param the BDD object
 */
void write_dd(std::shared_ptr<CuddManager> manager, const BDD& bdd, const char* filename)
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
        
        // Create BDD nodes vector with smart pointer management
        std::vector<BDD> nodes(xag->max_var + 1);
        nodes[0] = BDDFactory::createZero(manager);

        // Create input variables
        for (int i : xag->PI)
        {
            int ivar = aiger_var(i);
            nodes[ivar] = BDDFactory::createVar(manager);
        }

        // Process gates
        for (int gid : xag->used_gates)
        {
            Gate &g = xag->gates[gid];

            BDD i1 = nodes[aiger_var(g.inputs[0])];
            if (aiger_sign(g.inputs[0]))
                i1 = !i1;

            BDD i2 = nodes[aiger_var(g.inputs[1])];
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
        }

        ret_vals ret = ret_vals::ret_UNK;

        int po_var = aiger_var(xag->PO);
        if (po_var < 0 || po_var > xag->max_var || nodes[po_var].isNull())
        {
            printf("c [BDD] Error: Invalid PO variable index %d or null node, returning UNKNOWN\n", po_var);
            return ret_vals::ret_UNK;
        }

        assert(!nodes[po_var].isNull());

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

        printf("c [BDD] result = %d, [nodes = %ld, vars = %d, reorderings = %d, memory = %ld bytes] [time = %f] \n", 
            ret, 
            manager->readNodeCount(), 
            manager->readSize(),
            manager->readReorderings(),
            (long)manager->readMemoryInUse(), 
            fastLEC::ResMgr::get().get_runtime() - start_time);
        fflush(stdout);

        return ret;
    }
    catch (const std::exception& e)
    {
        printf("c [BDD] Error: %s, returning UNKNOWN\n", e.what());
        return ret_vals::ret_UNK;
    }
}
