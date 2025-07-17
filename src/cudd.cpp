#include <iostream>
#include <ios>
#include <cstring>
#include <sys/stat.h>
#include <sstream>

extern "C"
{
#include "../deps/cudd/config.h"
#include "../deps/cudd/util/util.h"
#include "../deps/cudd/cudd/cudd.h"
}

#include "fastLEC.hpp"

using namespace fastLEC;

/**
 * Print a dd summary
 * pr = 0 : prints nothing
 * pr = 1 : prints counts of nodes and minterms
 * pr = 2 : prints counts + disjoint sum of product
 * pr = 3 : prints counts + list of nodes
 * pr > 3 : prints counts + disjoint sum of product + list of nodes
 * @param the dd node
 */
void print_dd(DdManager *gbm, DdNode *dd, int n, int pr)
{
    printf("DdManager nodes: %ld | ", Cudd_ReadNodeCount(gbm));        /*Reports the number of live nodes in BDDs and ADDs*/
    printf("DdManager vars: %d | ", Cudd_ReadSize(gbm));               /*Returns the number of BDD variables in existence*/
    printf("DdManager reorderings: %d | ", Cudd_ReadReorderings(gbm)); /*Returns the number of times reordering has occurred*/
    printf("DdManager memory: %ld \n", Cudd_ReadMemoryInUse(gbm));     /*Returns the memory in use by the manager measured in bytes*/
    Cudd_PrintDebug(gbm, dd, n, pr);
}

/**
 * Writes a dot file representing the argument DDs
 * @param the node object
 */
void write_dd(DdManager *gbm, DdNode *dd, char *filename)
{
    char dir_path[256];
    strcpy(dir_path, filename);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash != NULL)
    {
        *last_slash = '\0';
#ifdef _WIN32
        _mkdir(dir_path);
#else
        mkdir(dir_path, 0755);
#endif
    }

    FILE *outfile; // output file pointer for .dot file
    outfile = fopen(filename, "w");
    DdNode **ddnodearray = (DdNode **)malloc(sizeof(DdNode *)); // initialize the function array
    ddnodearray[0] = dd;
    Cudd_DumpDot(gbm, 1, ddnodearray, NULL, NULL, outfile); // dump the function to .dot file
    free(ddnodearray);
    fclose(outfile); // close the file */
}

ret_vals fastLEC::Prove_Task::seq_bdd_cudd()
{

    double start_time = fastLEC::ResMgr::get().get_runtime();

    if (this->has_xag() == false)
    {
        printf("c [BDD] Error: XAG not found, returning UNKNOWN\n");
        return ret_vals::ret_UNK;
    }

    DdManager *gbm;
    gbm = Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
    if (!gbm)
    {
        printf("c [BDD] Error: Failed to initialize CUDD manager, returning UNKNOWN\n");
        return ret_vals::ret_UNK;
    }

    std::vector<DdNode *> nodes(xag->max_var + 1, nullptr);
    nodes[0] = Cudd_ReadLogicZero(gbm);

    for (int i : xag->PI)
    {
        int ivar = aiger_var(i);
        nodes[ivar] = Cudd_bddNewVar(gbm);
    }

    for (int gid : xag->used_gates)
    {
        Gate &g = xag->gates[gid];

        DdNode *i1 = nodes[aiger_var(g.inputs[0])];
        if (aiger_sign(g.inputs[0]))
            i1 = Cudd_Not(i1);

        DdNode *i2 = nodes[aiger_var(g.inputs[1])];
        if (aiger_sign(g.inputs[1]))
            i2 = Cudd_Not(i2);

        if (g.type == GateType::AND2)
        {
            nodes[aiger_var(g.output)] = Cudd_bddAnd(gbm, i1, i2);
        }
        else if (g.type == GateType::XOR2)
        {
            nodes[aiger_var(g.output)] = Cudd_bddXor(gbm, i1, i2);
        }
        Cudd_Ref(nodes[aiger_var(g.output)]);
    }

    ret_vals ret = ret_vals::ret_UNK;

    int po_var = aiger_var(xag->PO);
    if (po_var < 0 || po_var > xag->max_var || nodes[po_var] == nullptr)
    {
        printf("c [BDD] Error: Invalid PO variable index %d or null node, returning UNKNOWN\n", po_var);
        return ret_vals::ret_UNK;
    }

    assert(nodes[po_var] != nullptr);

    if (aiger_sign(xag->PO))
    {
        if (nodes[po_var] == Cudd_Not(Cudd_ReadLogicZero(gbm)))
            ret = ret_vals::ret_UNS;
        else
            ret = ret_vals::ret_SAT;
    }
    else
    {
        if (nodes[po_var] == Cudd_ReadLogicZero(gbm))
            ret = ret_vals::ret_UNS;
        else
            ret = ret_vals::ret_SAT;
    }

    int res = 0;
    if (ret == ret_vals::ret_SAT)
        res = 10;
    else if (ret == ret_vals::ret_UNS)
        res = 20;
    else
        res = 0;

    printf("c [BDD] result = %d, ", res);
    printf("[nodes = %ld,", Cudd_ReadNodeCount(gbm));
    printf(" vars = %d,", Cudd_ReadSize(gbm));
    printf(" reorderings = %d,", Cudd_ReadReorderings(gbm));
    printf(" memory = %ld bytes]", (long)Cudd_ReadMemoryInUse(gbm));
    printf(" [time = %f] \n", fastLEC::ResMgr::get().get_runtime() - start_time);

    for (int i = 0; i <= xag->max_var; i++)
    {
        if (nodes[i] != nullptr)
            Cudd_RecursiveDeref(gbm, nodes[i]);
    }

    Cudd_Quit(gbm);

    return ret;
}
