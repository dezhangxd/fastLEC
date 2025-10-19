#include "fastLEC.hpp"

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