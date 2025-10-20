#include "fastLEC.hpp"
#include <vector>

#include <thread>
#include <atomic>

fastLEC::engines
fastLEC::Prover::select_one_engine_hybridCEC(std::shared_ptr<fastLEC::XAG> xag)
{
    std::vector<int> bucket(xag->PI.size() + 1, 0);

    std::vector<std::vector<int>> XOR_blocks;
    std::vector<int> XOR_block_indices;
    xag->compute_XOR_blocks(XOR_blocks, XOR_block_indices);

    int mx = 0;
    for (auto &block : XOR_blocks)
    {
        bucket[block.size()]++;
        mx = std::max(mx, (int)block.size());
    }

    double score = mx;

    for (int i = 3; i < mx; i++)
    {
        bucket[i + 1] += bucket[i] >> 2;
        bucket[i] %= 2;
    }

    while (bucket[mx] >= 4)
        score++, bucket[mx] /= 4;
    for (int i = 1; i <= mx; i++)
        bucket[i] = 0;
    score /= 1.0 * xag->PI.size();

    if (xag->PI.size() <= 32 && score >= 0.3)
        return fastLEC::engines::engine_seq_ES;
    else
        return fastLEC::engines::engine_seq_SAT;
}

std::vector<int>
fastLEC::Prover::select_para_threads(std::shared_ptr<fastLEC::XAG> xag,
                                     int n_threads)
{
    if (xag->PI.size() <= 6)
        return {1, 0, 0}; // small instances using fast SAT check

    if (xag->PI.size() <= 18 + log2(n_threads))
        return {0, 1, 0}; // small instances that ES is fast enough

    if (n_threads == 1)
    {
        if (select_one_engine_hybridCEC(xag) == fastLEC::engines::engine_seq_ES)
            return {0, 1, 0};
        else
            return {1, 0, 0};
    }

    if (xag->PI.size() >= 36 + log2(n_threads))
    {
        if (n_threads > 1)
            return {n_threads - 1, 0, 1};
        else
            return {1, 0, 0};
    }

    // need to allocate instances
    int SAT_threads = 1;
    int ES_threads = 1;
    int BDD_threads = 1; // at most one thread

    double cost_SAT = 0.0;
    std::vector<std::vector<int>> XOR_blocks;
    std::vector<int> XOR_block_indices;
    xag->compute_XOR_blocks(XOR_blocks, XOR_block_indices);
    for (auto b : XOR_blocks)
        cost_SAT += std::pow(2, b.size());
    if (cost_SAT != 0.0)
        cost_SAT = 4 * std::log2(cost_SAT);

    double cost_ES = 0.0;
    cost_ES = std::log2(std::pow(2, xag->PI.size()) * xag->used_gates.size());
    

    if (cost_ES < cost_SAT)
    {
        if (1.5 * cost_ES < cost_SAT)
            return {0, n_threads, 0};
        else
        {
            int half = n_threads / 2;
            if (half == 0)
                ES_threads = 0;
            else
                ES_threads = 1 << (int)(log2(half));
            SAT_threads = n_threads - ES_threads;
            if (SAT_threads > 1)
            {
                SAT_threads -= 1;
                BDD_threads = 1;
            }
            return {SAT_threads, ES_threads, BDD_threads};
        }
    }
    else
    {
        return {n_threads, 0, 0};
    }

    return {SAT_threads, ES_threads, BDD_threads};
}
