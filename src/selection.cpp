#include "fastLEC.hpp"
#include <vector>
#include <algorithm>
#include <numeric>
#include <cassert>
#include <cstdio>
#include "XAG.hpp"
#include <algorithm>

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
fastLEC::Prover::select_heuristic_threads(std::shared_ptr<fastLEC::XAG> xag,
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

std::vector<int>
fastLEC::Prover::select_schedule_threads(std::shared_ptr<fastLEC::XAG> xag,
                                         int n_threads)
{

    if (xag->PI.size() <= 6)
        return {1, 0, 0}; // small instances using fast SAT check

    if (xag->PI.size() <= 18 + log2(n_threads))
        return {0, 1, 0}; // small instances that ES is fast enough

    if (n_threads == 1) // fast select one engine
    {
        if (select_one_engine_hybridCEC(xag) == fastLEC::engines::engine_seq_ES)
            return {0, 1, 0};
        else
            return {1, 0, 0};
    }

    if (!constructed)
    {
        if (XGBoosterCreate(nullptr, 0, &booster_sat) != 0)
        {
            std::cout << "c [Error] Failed to create booster" << std::endl;
            exit(0);
        }

        // Change the model file here.
        if (XGBoosterLoadModel(booster_sat, "src/model_sat.json") != 0)
        {
            std::cout << "c [Error] Failed to load SAT model" << std::endl;
            exit(0);
        }

        if (XGBoosterCreate(nullptr, 0, &booster_bdd) != 0)
        {
            std::cout << "c [Error] Failed to create booster" << std::endl;
            exit(0);
        }

        // Change the model file here.
        if (XGBoosterLoadModel(booster_bdd, "src/model_bdd.json") != 0)
        {
            std::cout << "c [Error] Failed to load BDD model" << std::endl;
            exit(0);
        }

        constructed = true;
    }

    std::vector<double> features_double = xag->generate_features();
    std::vector<float> features(features_double.begin(), features_double.end());
    assert(features.size() == 32); // 32 features

    // check NaN and inf
    for (size_t i = 0; i < features.size(); i++)
    {
        if (std::isnan(features[i]) || std::isinf(features[i]))
        {
            std::cout << "c [Warning] Feature " << i
                      << " is NaN or inf, setting to 0" << std::endl;
            features[i] = 0.0f;
        }
    }

    features.emplace_back(1.0); // predict 1 thread efficiency

    DMatrixHandle dtest_sat_local = nullptr;
    DMatrixHandle dtest_bdd_local = nullptr;

    if (XGDMatrixCreateFromMat(
            features.data(), 1, features.size(), -1e8, &dtest_sat_local) != 0)
    {
        std::cout << "c [Error] Failed to create DMatrix for SAT" << std::endl;
        exit(0);
    }

    if (XGDMatrixCreateFromMat(
            features.data(), 1, features.size(), -1e8, &dtest_bdd_local) != 0)
    {
        std::cout << "c [Error] Failed to create DMatrix for BDD" << std::endl;
        XGDMatrixFree(dtest_sat_local);
        exit(0);
    }

    bst_ulong out_len;
    const float *out_result_sat = nullptr;
    const float *out_result_bdd = nullptr;

    if (XGBoosterPredict(
            booster_sat, dtest_sat_local, 0, 0, 0, &out_len, &out_result_sat) !=
        0)
    {
        std::cout << "c [Error] Prediction SAT failed: " << XGBGetLastError()
                  << std::endl;
        XGDMatrixFree(dtest_sat_local);
        XGDMatrixFree(dtest_bdd_local);
        exit(0);
    }

    if (XGBoosterPredict(
            booster_bdd, dtest_bdd_local, 0, 0, 0, &out_len, &out_result_bdd) !=
        0)
    {
        std::cerr << "c [Error] Prediction BDD failed: " << XGBGetLastError()
                  << std::endl;
        XGDMatrixFree(dtest_sat_local);
        XGDMatrixFree(dtest_bdd_local);
        exit(0);
    }

    float pred_SAT = out_result_sat[0];
    float pred_BDD = out_result_bdd[0];
    float pred_ES = 0.0;

    auto clamp = [](float val, float min_val, float max_val)
    {
        return std::max(min_val, std::min(val, max_val));
    };

    // pred 1t SAT and BDD, pred nthread time for ES.
    // SAT is increased with log scale
    // ES is increased with linear scale
    pred_SAT =
        clamp(pred_SAT, 0.0f, static_cast<float>(2 * Param::get().timeout));
    pred_BDD =
        clamp(pred_BDD, 0.0f, static_cast<float>(2 * Param::get().timeout));
    pred_ES = 0.0003 * xag->used_gates.size() *
        std::pow(2, (xag->PI.size() - log2(n_threads) - 23));

    printf("c [Schedule] predicted time: SAT=%.3f, ES=%.3f, BDD=%.3f\n",
           pred_SAT,
           pred_ES,
           pred_BDD);
    fflush(stdout);

    bool can_use_ES = false;
    if (pred_ES < 1.5 * Param::get().timeout)
        can_use_ES = true;

    bool can_use_BDD = false;
    const float bdd_margin = 0.8f;
    if (pred_BDD < pred_SAT * bdd_margin && n_threads >= 2)
        can_use_BDD = true;

    XGDMatrixFree(dtest_sat_local);
    XGDMatrixFree(dtest_bdd_local);

    // Set the thread allocation strategy based on can_use_BDD and can_use_ES
    int n_threads_SAT = 0;
    int n_threads_ES = 0;
    int n_threads_BDD = 0;

    // at most 1 thread for BDD (when enable), otherwise 0
    if (can_use_BDD)
        n_threads_BDD = 1;
    else
        n_threads_BDD = 0;

    int remain_threads = n_threads - n_threads_BDD;

    // can use ES
    if (can_use_ES && remain_threads >= 2)
    {
        // allocate SAT and ES threads, at least 1 thread for each
        if (pred_SAT < 0.5 * pred_ES)
        {
            // more for SAT
            n_threads_SAT = std::max(1, remain_threads - 1);
            n_threads_ES = remain_threads - n_threads_SAT;
        }
        else if (pred_SAT < 2 * pred_ES)
        {
            // half
            n_threads_SAT = std::max(1, remain_threads / 2);
            n_threads_ES = remain_threads - n_threads_SAT;
        }
        else
        {
            // more for ES
            n_threads_ES = std::max(1, remain_threads - 1);
            n_threads_SAT = remain_threads - n_threads_ES;
        }
    }
    else if (can_use_ES && remain_threads == 1)
    {
        // one ES or one SAT
        if (pred_SAT < pred_ES)
        {
            n_threads_SAT = 1;
            n_threads_ES = 0;
        }
        else
        {
            n_threads_SAT = 0;
            n_threads_ES = 1;
        }
    }
    else // only can use SAT
    {
        n_threads_SAT = remain_threads;
        n_threads_ES = 0;
    }

    if (n_threads_SAT < 0)
        n_threads_SAT = 0;
    if (n_threads_ES < 0)
        n_threads_ES = 0;
    if (n_threads_BDD < 0)
        n_threads_BDD = 0;
    while (n_threads_SAT + n_threads_ES + n_threads_BDD < n_threads)
    {
        n_threads_SAT++;
    }
    while (n_threads_SAT + n_threads_ES + n_threads_BDD > n_threads)
    {
        if (n_threads_SAT > 0)
            n_threads_SAT--;
        else if (n_threads_ES > 0)
            n_threads_ES--;
        else if (n_threads_BDD > 0)
            n_threads_BDD = 0;
    }

    printf("c [Schedule] thread assign: SAT=%d, ES=%d, BDD=%d\n",
           n_threads_SAT,
           n_threads_ES,
           n_threads_BDD);
    return {n_threads_SAT, n_threads_ES, n_threads_BDD};
}

std::vector<int>
fastLEC::Prover::select_half_threads(std::shared_ptr<fastLEC::XAG> xag [[maybe_unused]],
                                     int n_threads)
{
    int n_threads_SAT = std::max(1, n_threads / 2);
    int n_threads_ES = std::max(0, n_threads - n_threads_SAT);
    int n_threads_BDD = 0;
    if (n_threads_ES > 1)
        n_threads_ES -= 1, n_threads_BDD = 1;
    return {n_threads_SAT, n_threads_ES, n_threads_BDD};
}

std::vector<int>
fastLEC::Prover::select_gpu_threads(std::shared_ptr<fastLEC::XAG> xag [[maybe_unused]],
                                     int n_threads)
{
    if (xag->PI.size() <= 6)
        return {1, 0, 0}; // small instances using fast SAT check

    double pred_ES = 0.0003 * xag->used_gates.size() *
        std::pow(2, (xag->PI.size() - log2(n_threads) - 23));

    if (pred_ES < 0.1)
        return {0, n_threads, 0}; // CPU ES is fast enough; 

    
    if (xag->PI.size() <= 26)
        return {0, 1, 0}; // small instances that ES is fast enough

    if (n_threads == 1)
    {
        if (select_one_engine_hybridCEC(xag) == fastLEC::engines::engine_seq_ES)
            return {0, 1, 0};
        else
            return {1, 0, 0};
    }

    bool use_gpuES = false;
    // assume 4090 is similar to 128 threads for ES
    double pred_ES_4090 = 0.0003 * xag->used_gates.size() *
    std::pow(2, (xag->PI.size() - log2(128) - 23)); 

    if(pred_ES_4090 <= 1.2 * Param::get().timeout)
        use_gpuES = true;

    return {n_threads - 1, use_gpuES ? -1 : 0, 1};
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
    (void) min_idis;
    // features.push_back(min_idis);
    features.push_back(avg_idis);
    features_names.push_back("max_idis");
    // features_names.push_back("min_idis");
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
    (void) min_odis;
    // features.push_back(min_odis);
    features.push_back(avg_odis);
    features_names.push_back("max_odis");
    // features_names.push_back("min_odis");
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
    (void) min_out_degree;
    // features.push_back(min_out_degree);
    features.push_back(avg_out_degree);
    features_names.push_back("max_out_degree");
    // features_names.push_back("min_out_degree");
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