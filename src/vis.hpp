#pragma once

#include "XAG.hpp"
#include "CNF.hpp"
#include <memory>
#include <vector>
#include <string>

namespace fastLEC
{

class Visualizer
{
public:
    std::shared_ptr<fastLEC::XAG> xag;
    std::shared_ptr<fastLEC::CNF> cnf;

    Visualizer(std::shared_ptr<fastLEC::XAG> xag);
    ~Visualizer() = default;

    std::string gen_basename_str();
    std::string gen_basename_str(std::vector<int> unit_clauses);

    void call_external_program_for_runtime_generation();

    struct dot_data
    {
        std::string dot_filename;
        double base_runtime;
        std::vector<double> pos_runtimes, neg_runtimes;
        std::vector<bool> mask;

        // related to scores
        std::vector<double> speedups, scores;
        std::vector<int> speedup_rank, score_rank;
        std::vector<std::vector<int>> XOR_chains, important_nodes;
        std::vector<bool> is_important;
        std::vector<int> in_degree, out_degree, idis, odis;
        std::vector<std::vector<int>> n_step_XOR_cnt, n_step_Gates_cnt;
    };

    void re_compute_scores(dot_data &dot_data);

    void generate_dot(dot_data &dot_data);

    void visualize(std::vector<int> unit_clauses);
};

} // namespace fastLEC