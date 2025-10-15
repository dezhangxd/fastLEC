#include "pSAT.hpp"
#include "basic.hpp"
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
    compute_scores();
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

bool fastLEC::PartitionSAT::compute_scores()
{
    for (int i = 1; i <= root_cnf->num_vars; i++)
    {
        scores[i] = 1.0 + fastLEC::ResMgr::get().random_double(0.0, 1.0);
        scores[227] += 1000.0;
        scores[243] += 1000.0;
        // printf("score[%d] = %f\n", i, scores[i]);
        // fflush(stdout);
    }
    return true;
}
