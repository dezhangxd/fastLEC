#include "pSAT.hpp"
#include "XAG.hpp"
#include "parser.hpp"

int fastLEC::PartitionSAT::decide_split_vars()
{
    int ct = 1;
    double nt = n_threads + 1.0;
    double np = running_cpu_cnt + 1.0;
    if (np - nt >= 8 && np / nt >= 4.0)
        ct = 3;
    else if (np - nt >= 4 && np / nt >= 1.5)
        ct = 2;
    else
        ct = 1;

    return ct;
}

std::shared_ptr<fastLEC::Task> fastLEC::PartitionSAT::pick_split_task()
{
    unsigned max_split = fastLEC::Param::get().custom_params.gt_max_split;
    double max_rt = -1;
    std::shared_ptr<Task> target_task = nullptr;

    for (unsigned i = 0; i < n_threads; i++)
    {
        auto task = get_task_by_cpu(i);
        if (task != nullptr)
        {
            // score considering:
            // 1. runtime (the larger, the better)
            // 2. level (better to be the leaves)
            // 3.1. split times (do not split a task too many times) allow at
            // most 3.2. number of sons (the less, the better)

            if (task->split_ct() >= max_split)
                continue;

            // 1
            double score = task->runtime();
            // 2
            double lev_coe =
                fastLEC::Param::get().custom_params.gt_level_coefficient;
            score = score * pow(lev_coe, task->level);
            // 3.1 + 3.2
            double spl_dec = fastLEC::Param::get().custom_params.gt_split_decay;
            double son_dec = fastLEC::Param::get().custom_params.gt_sons_decay;
            double spl_ratio = 1.0;
            for (auto &group : task->sons)
                spl_ratio += log2(group.size() * son_dec);
            score = score * pow(spl_dec, spl_ratio);

            // TODO: consider to use a probability distribution for the runtime.

            if (score > max_rt)
            {
                max_rt = score;
                target_task = task;
            }
        }
        // printf("c [split] task = %d\n", target_task->id);
    }
    if (target_task == nullptr || target_task->is_solved())
        return nullptr;
    else
        return target_task;
}

void fastLEC::PartitionSAT::compute_scores(const std::vector<bool> &mask,
                                           std::vector<double> &scores)
{
    std::vector<std::vector<int>> XOR_chains;
    std::vector<std::vector<int>> important_nodes;
    this->xag->compute_XOR_chains(mask, XOR_chains, important_nodes);

    std::vector<int> in_degree;
    std::vector<int> out_degree;
    std::vector<int> idis;
    std::vector<int> odis;
    this->xag->compute_distance(mask, in_degree, out_degree, idis, odis);

    scores.resize(root_cnf->num_vars + 1, 1.0);

    for (unsigned i = 0; i < XOR_chains.size(); i++)
    {
        for (auto v : XOR_chains[i])
        {
            double alpha = 0.6;
            double dis = alpha * odis[v] + (1 - alpha) * idis[v] + 1.0;
            scores[v] = std::pow(XOR_chains[i].size(), 2.0) *
                log2(root_cnf->num_vars) / dis;
        }
        for (auto v : important_nodes[i])
        {
            scores[v] *= 10.0;
        }
    }

    int prop_lev = 2;
    double beta = 10.0;
    for (int i = 0; i < prop_lev; i++)
    {
        std::vector<double> score_bac = scores;
        for (int v = 1; v <= root_cnf->num_vars; v++)
        {
            if (!mask[v])
            {
                // 计算v的邻居的平均得分
                double sum = 0.0;
                double cnt = 0;
                for (auto u : this->xag->v_usr[v])
                {
                    if (!mask[u])
                    {
                        sum += score_bac[u];
                        cnt += 1;
                    }
                }
                int i1 = aiger_var(this->xag->gates[v].inputs[0]);
                int i2 = aiger_var(this->xag->gates[v].inputs[1]);
                if (!mask[i1])
                {
                    sum += score_bac[i1];
                    cnt += 1;
                }
                if (!mask[i2])
                {
                    sum += score_bac[i2];
                    cnt += 1;
                }

                sum += beta * cnt * score_bac[v];
                cnt += beta * cnt;

                scores[v] = sum / cnt;
            }
        }
    }
}
