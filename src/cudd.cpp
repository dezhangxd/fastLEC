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
    // 检查并创建目录
    char dir_path[256];
    strcpy(dir_path, filename);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash != NULL)
    {
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
    char filename[256];  // 增加缓冲区大小
    DdManager *gbm;
    
    // 检查CUDD是否正确初始化
    gbm = Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
    if (!gbm) {
        printf("c [BDD] Error: Failed to initialize CUDD manager, returning UNKNOWN\n");
        return ret_vals::ret_UNK;
    }

    DdNode *a = Cudd_bddNewVar(gbm);
    DdNode *b = Cudd_bddNewVar(gbm);
    
    // 第一个半加器的sum和carry
    DdNode *sum1 = Cudd_bddXor(gbm, a, b);
    Cudd_Ref(sum1);
    DdNode *carry1 = Cudd_bddAnd(gbm, a, b);
    Cudd_Ref(carry1);

    // 第二个半加器的sum和carry
    DdNode *nota = Cudd_Not(a);
    DdNode *notb = Cudd_Not(b);
    DdNode *na = Cudd_bddNand(gbm, nota, b);
    Cudd_Ref(na);
    DdNode *nb = Cudd_bddNand(gbm, a, notb);
    Cudd_Ref(nb);
    DdNode *sum2 = Cudd_bddNand(gbm, na, nb);
    Cudd_Ref(sum2);
    DdNode *tmp = Cudd_bddNand(gbm, a, b);
    Cudd_Ref(tmp);
    DdNode *carry2 = Cudd_Not(tmp);
    Cudd_Ref(carry2);

    // 比较两个半加器的输出
    DdNode *xor1 = Cudd_bddXor(gbm, sum1, sum2);
    Cudd_Ref(xor1);
    DdNode *xor2 = Cudd_bddXor(gbm, carry1, carry2);
    Cudd_Ref(xor2);

    DdNode *bdd = Cudd_bddOr(gbm, xor1, xor2);
    Cudd_Ref(bdd);
    
    // 转换为ADD用于显示
    DdNode *bdd_add = Cudd_BddToAdd(gbm, bdd);
    Cudd_Ref(bdd_add);

    print_dd(gbm, bdd_add, 2, 4);
    sprintf(filename, "./logs_bdd/graph.dot");
    write_dd(gbm, bdd_add, filename);
    
    // 判断结果：简化逻辑
    ret_vals result = (Cudd_IsComplement(bdd) || bdd == Cudd_ReadLogicZero(gbm)) ? 
                      ret_vals::ret_UNS : ret_vals::ret_SAT;
    
    printf("c [BDD] Result: %s\n", 
           (result == ret_vals::ret_UNS) ? "EQUIVALENT" : "NOT EQUIVALENT");
    
    // 正确释放所有引用的节点
    Cudd_RecursiveDeref(gbm, bdd_add);
    Cudd_RecursiveDeref(gbm, bdd);
    Cudd_RecursiveDeref(gbm, xor2);
    Cudd_RecursiveDeref(gbm, xor1);
    Cudd_RecursiveDeref(gbm, carry2);
    Cudd_RecursiveDeref(gbm, tmp);
    Cudd_RecursiveDeref(gbm, sum2);
    Cudd_RecursiveDeref(gbm, nb);
    Cudd_RecursiveDeref(gbm, na);
    Cudd_RecursiveDeref(gbm, carry1);
    Cudd_RecursiveDeref(gbm, sum1);
    
    Cudd_Quit(gbm);

    return result;
}