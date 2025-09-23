#pragma once

#include <map>

#include "XAG.hpp"
#include "basic.hpp"

namespace fastLEC
{

// SAT sweeping engines
class Sweeper
{
    std::shared_ptr<fastLEC::XAG> xag;

    // eql classes
    unsigned next_class_idx = 0;
    // the index of a aiger literal in eql_class
    std::vector<int> class_index;
    // the potential-eql node pairs (aiger literal pair)
    std::vector<std::vector<int>> eql_classes;
    // the skipped pairs
    std::map<int, std::vector<std::pair<int, int>>> skip_pairs;
    // the proved pairs
    std::vector<std::pair<int, int>> proved_pairs;
    // the rejected pairs
    std::vector<std::pair<int, int>> rejected_pairs;

public:
    Sweeper() = default;
    Sweeper(std::shared_ptr<fastLEC::XAG> xag) : xag(xag) {}
    ~Sweeper() = default;

    void clear();

    fastLEC::ret_vals logic_simulation();

    std::string sub_graph_string;
    // get the next sub-graph in XAG format
    std::shared_ptr<fastLEC::XAG> next_sub_graph();
    // the nodes in the last class are proven to be equivalent or not equivalent
    void post_proof(fastLEC::ret_vals ret);
};

} // namespace fastLEC