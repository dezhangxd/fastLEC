#include <iostream>
#include <ios>
#include <cstring>
#include <sys/stat.h>

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
    Cudd_PrintDebug(gbm, dd, n, pr);                                   // Prints to the standard output a DD and its statistics: number of nodes, number of leaves, number of minterms.
}

/**
 * Writes a dot file representing the argument DDs
 * @param the node object
 */
void write_dd(DdManager *gbm, DdNode *dd, char *filename)
{
    // 检查并创建目录
    char dir_path[256];
    strcpy(dir_path, filename);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash != NULL) {
        *last_slash = '\0';
        // 创建目录（如果不存在）
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

    char filename[30];
    DdManager *gbm; /* Global BDD manager. */
    gbm = Cudd_Init(0,0,CUDD_UNIQUE_SLOTS,CUDD_CACHE_SLOTS,0); /* Initialize a new BDD manager. */
    DdNode *bdd, *x1, *x2;
    x1 = Cudd_bddNewVar(gbm); /*Create a new BDD variable x1*/
    x2 = Cudd_bddNewVar(gbm); /*Create a new BDD variable x2*/
    bdd = Cudd_bddXor(gbm, x1, x2); /*Perform XOR Boolean operation*/
    Cudd_Ref(bdd);          /*Update the reference count for the node just created.*/
    bdd = Cudd_BddToAdd(gbm, bdd); /*Convert BDD to ADD for display purpose*/
    print_dd (gbm, bdd, 2,4);   /*Print the dd to standard output*/
    sprintf(filename, "./logs_bdd/graph.dot"); /*Write .dot filename to a string*/
    write_dd(gbm, bdd, filename);  /*Write the resulting cascade dd to a file*/
    Cudd_Quit(gbm);

    return ret_vals::ret_UNS;
}