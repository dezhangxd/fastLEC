#include "fastLEC.hpp"
#include <vector>
#include <algorithm>
#include <numeric>
#include "XAG.hpp"

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

std::vector<double> fastLEC::XAG::generate_features()
{
    std::vector<double> features;
    features_names.clear();

    // 1.1 number of inputs
    features.push_back(this->PI.size());
    features_names.push_back("num_PIs");
    // 1.2 number of gates
    features.push_back(this->used_gates.size());
    features_names.push_back("num_used_gates");
    // 1.3 number of XOR gates
    // 1.4 number of AND gates
    int XOR_gates = 0, AND_gates = 0;
    for (auto gate : this->used_gates)
    {
        if (this->gates[gate].type == fastLEC::GateType::XOR2)
            XOR_gates++;
        else if (this->gates[gate].type == fastLEC::GateType::AND2)
            AND_gates++;
    }
    features.push_back(XOR_gates);
    features_names.push_back("num_XOR_gates");
    features.push_back(AND_gates);
    features_names.push_back("num_AND_gates");

    // 2.1 CNF variable number
    // 2.2 CNF clause number
    // 2.3 CNF literals number
    auto cnf = this->construct_cnf_from_this_xag();
    features.push_back(cnf->num_vars);
    features_names.push_back("num_CNF_vars");
    features.push_back(cnf->num_clauses());
    features_names.push_back("num_CNF_clauses");
    features.push_back(cnf->num_lits());
    features_names.push_back("num_CNF_lits");

    // 3.1 XOR blocks: min, max, avg, geo_mean
    std::vector<std::vector<int>> XOR_blocks;
    std::vector<int> XOR_block_indices;
    this->compute_XOR_blocks(XOR_blocks, XOR_block_indices);

    int min_block = XOR_blocks.empty() ? 0 : XOR_blocks[0].size();
    int max_block = 0;
    double sum_block = 0.0;
    double prod_block = 1.0;

    for (const auto &block : XOR_blocks)
    {
        int sz = block.size();
        if (sz < min_block)
            min_block = sz;
        if (sz > max_block)
            max_block = sz;
        sum_block += sz;
        prod_block *= (sz > 0) ? sz : 1; // avoid multiplying by 0
    }
    double avg_block = XOR_blocks.empty() ? 0.0 : sum_block / XOR_blocks.size();
    double geo_mean_block =
        XOR_blocks.empty() ? 0.0 : pow(prod_block, 1.0 / XOR_blocks.size());

    features.push_back(min_block);
    features_names.push_back("min_XOR_block");
    features.push_back(max_block);
    features_names.push_back("max_XOR_block");
    features.push_back(avg_block);
    features_names.push_back("avg_XOR_block");
    features.push_back(geo_mean_block);
    features_names.push_back("geo_mean_XOR_block");

    // 3.2 XOR chains: min, max, avg, geo_mean
    std::vector<std::vector<int>> XOR_chains;
    std::vector<std::vector<int>> important_nodes;
    std::vector<bool> mask(this->max_var + 1, false);
    this->compute_XOR_chains(mask, XOR_chains, important_nodes);

    int min_chain = XOR_chains.empty() ? 0 : -1;
    int max_chain = 0;
    double sum_chain = 0.0;
    double prod_chain = 1.0;
    for (const auto &chain : XOR_chains)
    {
        int sz = chain.size();
        if (min_chain == -1 || sz < min_chain)
            min_chain = sz;
        if (sz > max_chain)
            max_chain = sz;
        sum_chain += sz;
        prod_chain *= (sz > 0 ? sz : 1); // to avoid multiplying by 0
    }

    double avg_chain = XOR_chains.empty() ? 0.0 : sum_chain / XOR_chains.size();
    double geo_mean_chain =
        XOR_chains.empty() ? 0.0 : pow(prod_chain, 1.0 / XOR_chains.size());

    features.push_back(min_chain == -1 ? 0 : min_chain);
    features_names.push_back("min_XOR_chain");
    features.push_back(max_chain);
    features_names.push_back("max_XOR_chain");
    features.push_back(avg_chain);
    features_names.push_back("avg_XOR_chain");
    features.push_back(geo_mean_chain);
    features_names.push_back("geo_mean_XOR_chain");

    // 4.1 distances to inputs and outputs
    std::vector<int> in_degree;
    std::vector<int> out_degree;
    std::vector<int> idis;
    std::vector<int> odis;
    this->compute_distance(mask, in_degree, out_degree, idis, odis);

    double max_idis =
        idis.size() < 1 ? 0.0 : *std::max_element(idis.begin() + 1, idis.end());
    double min_idis =
        idis.size() < 1 ? 0.0 : *std::min_element(idis.begin() + 1, idis.end());
    double avg_idis = idis.size() < 1
        ? 0.0
        : std::accumulate(idis.begin() + 1, idis.end(), 0.0) /
            (idis.size() - 1);

    features.push_back(max_idis);
    features.push_back(min_idis);
    features.push_back(avg_idis);
    features_names.push_back("max_idis");
    features_names.push_back("min_idis");
    features_names.push_back("avg_idis");

    double max_odis =
        odis.size() < 1 ? 0.0 : *std::max_element(odis.begin() + 1, odis.end());
    double min_odis =
        odis.size() < 1 ? 0.0 : *std::min_element(odis.begin() + 1, odis.end());
    double avg_odis = odis.size() < 1
        ? 0.0
        : std::accumulate(odis.begin() + 1, odis.end(), 0.0) /
            (odis.size() - 1);

    features.push_back(max_odis);
    features.push_back(min_odis);
    features.push_back(avg_odis);
    features_names.push_back("max_odis");
    features_names.push_back("min_odis");
    features_names.push_back("avg_odis");

    double max_sum_dis = 0.0, min_sum_dis = 0.0, avg_sum_dis = 0.0;
    if (!idis.empty() && !odis.empty() && idis.size() == odis.size())
    {
        min_sum_dis = std::numeric_limits<double>::max();
        max_sum_dis = std::numeric_limits<double>::lowest();
        double sum = 0.0;
        int count = 0;
        // Start from index 1 to skip constant/invalid node 0
        for (size_t i = 1; i < idis.size() - 1; ++i)
        {
            double cur_sum = idis[i] + odis[i];
            if (cur_sum > max_sum_dis)
                max_sum_dis = cur_sum;
            if (cur_sum < min_sum_dis)
                min_sum_dis = cur_sum;
            sum += cur_sum;
            ++count;
        }
        avg_sum_dis = count > 0 ? sum / count : 0.0;
        if (count == 0)
        {
            max_sum_dis = 0.0;
            min_sum_dis = 0.0;
            avg_sum_dis = 0.0;
        }
    }

    features.push_back(max_sum_dis);
    features.push_back(min_sum_dis);
    features.push_back(avg_sum_dis);
    features_names.push_back("max_sum_dis");
    features_names.push_back("min_sum_dis");
    features_names.push_back("avg_sum_dis");

    // 4.2 out-degrees
    double max_out_degree = out_degree.size() < 2
        ? 0.0
        : *std::max_element(out_degree.begin() + 1, out_degree.end() - 1);
    double min_out_degree = out_degree.size() < 2
        ? 0.0
        : *std::min_element(out_degree.begin() + 1, out_degree.end() - 1);
    double avg_out_degree = out_degree.size() < 2
        ? 0.0
        : std::accumulate(out_degree.begin() + 1, out_degree.end() - 1, 0.0) /
            (out_degree.size() - 2);

    features.push_back(max_out_degree);
    features.push_back(min_out_degree);
    features.push_back(avg_out_degree);
    features_names.push_back("max_out_degree");
    features_names.push_back("min_out_degree");
    features_names.push_back("avg_out_degree");

    // 5.1 cost SAT
    double cost_SAT = 0.0;
    for (auto b : XOR_blocks)
        cost_SAT += std::pow(2, b.size());
    if (cost_SAT != 0.0)
        cost_SAT = 4 * std::log2(cost_SAT);
    features.push_back(cost_SAT);
    features_names.push_back("cost_SAT");

    // 5.2 cost ES
    double cost_ES = 0.0;
    cost_ES = std::log2(std::pow(2, this->PI.size()) * this->used_gates.size());
    features.push_back(cost_ES);
    features_names.push_back("cost_ES");

    // 6 simulation features
    std::vector<double> stable_percentages;
    std::vector<double> entropys;
    this->compute_simulation_features(stable_percentages, entropys);
    double min_stable_percentage = stable_percentages.size() < 2
        ? 0.0
        : *std::min_element(stable_percentages.begin() + 1,
                            stable_percentages.end() - 1);
    double max_stable_percentage = stable_percentages.size() < 2
        ? 0.0
        : *std::max_element(stable_percentages.begin() + 1,
                            stable_percentages.end() - 1);
    double avg_stable_percentage = stable_percentages.size() < 2
        ? 0.0
        : std::accumulate(stable_percentages.begin() + 1,
                          stable_percentages.end() - 1,
                          0.0) /
            (stable_percentages.size() - 2);
    features.push_back(min_stable_percentage);
    features.push_back(max_stable_percentage);
    features.push_back(avg_stable_percentage);
    features_names.push_back("min_stable_percentage");
    features_names.push_back("max_stable_percentage");
    features_names.push_back("avg_stable_percentage");

    double min_entropy = entropys.size() < 2
        ? 0.0
        : *std::min_element(entropys.begin() + 1, entropys.end() - 1);
    double max_entropy = entropys.size() < 2
        ? 0.0
        : *std::max_element(entropys.begin() + 1, entropys.end() - 1);
    double avg_entropy = entropys.size() < 2
        ? 0.0
        : std::accumulate(entropys.begin() + 1, entropys.end() - 1, 0.0) /
            (entropys.size() - 2);

    features.push_back(min_entropy);
    features.push_back(max_entropy);
    features.push_back(avg_entropy);
    features_names.push_back("min_entropy");
    features_names.push_back("max_entropy");
    features_names.push_back("avg_entropy");

    return features;
}