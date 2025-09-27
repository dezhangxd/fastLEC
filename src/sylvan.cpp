#include "fastLEC.hpp"
#include "parser.hpp"

extern "C" {
#include "../deps/sylvan/src/sylvan.h"
}

#include "../deps/sylvan/src/sylvan_obj.hpp"

fastLEC::ret_vals fastLEC::Prover::para_BDD_sylvan(std::shared_ptr<fastLEC::XAG> xag, int n_t)
{
    double start_time = fastLEC::ResMgr::get().get_runtime();

    if (xag == nullptr)
    {
        printf("c [Sylvan] Error: XAG not found, returning UNKNOWN\n");
        return ret_vals::ret_UNK;
    }

    // 确保线程数至少为1
    if (n_t <= 0) n_t = 1;

    try
    {
        // 初始化 Lace 框架（如果尚未初始化）
        static bool lace_initialized = false;
        if (!lace_initialized) {
            lace_start(n_t, 0);  // n_t 个工作者线程
            lace_initialized = true;
        }

        // 初始化 Sylvan
        static bool sylvan_initialized = false;
        if (!sylvan_initialized) {
            // 设置 Sylvan 的大小参数
            sylvan::sylvan_set_sizes(1LL<<20, 1LL<<20, 1LL<<20, 1LL<<20);  // 1M 节点
            sylvan::sylvan_init_package();
            sylvan::sylvan_init_bdd();
            sylvan_initialized = true;
        }

        // 创建 BDD 节点向量
        std::vector<sylvan::Bdd> nodes(xag->max_var + 1);
        nodes[0] = sylvan::Bdd::bddZero();  // 节点0代表false

        // 为输入变量创建 BDD
        for (int i : xag->PI)
        {
            int ivar = aiger_var(i);
            nodes[ivar] = sylvan::Bdd::bddVar(ivar);
        }

        // 处理门电路
        bool timeout_detected = false;
        int gate_count = 0;
        const int check_interval = 100; // 每100个门检查一次超时

        for (int gid : xag->used_gates)
        {
            // 检查超时
            if (gate_count % check_interval == 0) {
                double current_time = fastLEC::ResMgr::get().get_runtime();
                if (current_time > Param::get().timeout) {
                    timeout_detected = true;
                    break;
                }
            }

            Gate &g = xag->gates[gid];

            // 获取输入 BDD
            sylvan::Bdd i1 = nodes[aiger_var(g.inputs[0])];
            if (aiger_sign(g.inputs[0]))
                i1 = !i1;

            sylvan::Bdd i2 = nodes[aiger_var(g.inputs[1])];
            if (aiger_sign(g.inputs[1]))
                i2 = !i2;

            // 根据门类型执行操作
            if (g.type == GateType::AND2)
            {
                nodes[aiger_var(g.output)] = i1 & i2;
            }
            else if (g.type == GateType::XOR2)
            {
                nodes[aiger_var(g.output)] = i1 ^ i2;
            }

            gate_count++;
        }

        ret_vals ret = ret_vals::ret_UNK;

        // 最终超时检查
        double final_time = fastLEC::ResMgr::get().get_runtime();
        bool final_timeout = timeout_detected || (final_time > Param::get().timeout);

        if (final_timeout) {
            ret = ret_vals::ret_UNK;
        } else {
            // 正常结果评估
            int po_var = aiger_var(xag->PO);
            if (po_var < 0 || po_var > xag->max_var)
            {
                ret = ret_vals::ret_UNK;
            }
            else
            {
                sylvan::Bdd po_bdd = nodes[po_var];
                if (aiger_sign(xag->PO))
                {
                    // 输出被取反
                    if (po_bdd == sylvan::Bdd::bddZero())
                        ret = ret_vals::ret_UNS;  // 输出为1，满足
                    else
                        ret = ret_vals::ret_SAT;  // 输出为0，不满足
                }
                else
                {
                    // 输出正常
                    if (po_bdd == sylvan::Bdd::bddZero())
                        ret = ret_vals::ret_UNS;  // 输出为0，不满足
                    else
                        ret = ret_vals::ret_SAT;  // 输出为1，满足
                }
            }
        }

        // 输出统计信息
        bool timed_out = final_timeout;
        const char* timeout_status = timed_out ? " (TO)" : "";
        
        printf("c [Sylvan] result = %d, [threads = %d, time = %f]%s \n", 
            ret, 
            n_t,
            fastLEC::ResMgr::get().get_runtime() - start_time,
            timeout_status);
        fflush(stdout);

        return ret;
    }
    catch (const std::exception& e)
    {
        printf("c [Sylvan] Error: %s, returning UNKNOWN\n", e.what());
        return ret_vals::ret_UNK;
    }
}