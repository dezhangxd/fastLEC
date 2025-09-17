#pragma once

#include <memory>
#include <vector>
#include <iostream>
#include "AIG.hpp"
#include "CNF.hpp"

namespace fastLEC
{

enum GateType
{
    NUL,  // unknown gate
    AND2, // AND gate with 2 inputs
    XOR2, // XOR gate with 2 inputs
    PI,   // primary input
};

class Gate
{
public:
    Gate() = default;
    ~Gate() = default;

    Gate(int o,
         fastLEC::GateType type = fastLEC::GateType::NUL,
         int i0 = -1,
         int i1 = -1)
        : output(o), type(type), inputs{i0, i1}
    {
    }

    int output; // AIG literal
    fastLEC::GateType type;
    int inputs[2]; // AIG literals

    void set(int o,
             fastLEC::GateType type = fastLEC::GateType::NUL,
             int i0 = -1,
             int i1 = -1)
    {
        this->output = o;
        this->type = type;
        this->inputs[0] = i0;
        this->inputs[1] = i1;
    }
};

class XAG
{
public:
    XAG() = default;
    explicit XAG(const fastLEC::AIG &aig) { construct_from_aig(aig); }
    ~XAG() = default;

    //---------------------------------------------------
    // XAG literals in AIG
    //---------------------------------------------------
    int max_var;         // the max variable number in the XAG
    int num_PIs_org;     // the number of PIs in the original AIG
    std::vector<int> PI; // the used PIs (aiger literals)
    int PO;              // the PO (or const 0, 1)

    //---------------------------------------------------
    // gates
    //---------------------------------------------------
    std::vector<Gate> gates;     // the gates (only considering AND and XOR)
    std::vector<int> used_gates; // the used gates (AIG variable) in the XAG
    std::vector<bool> used_lits; // whether (AIG) literals are used in the XAG

    //---------------------------------------------------
    // topological sorting
    //---------------------------------------------------
    std::vector<int>
        topo_idx; // the topological sorting index for (AIG variable) of the XAG
    std::vector<std::vector<int>> v_usr; // users (AIG variables) of a XAG
                                         // variable (not consider mediate var)
    void topological_sort(); // compute topo_idx for all the used gates

    //---------------------------------------------------
    // strash hashing
    //---------------------------------------------------
    std::vector<int> varcone_sizes;
    void fast_compute_varcone_sizes(); // compute cone sizes for all variables
    bool strash_prune(unsigned a, unsigned b); // strash hashing matching

    //---------------------------------------------------
    // sub-graph extraction
    //---------------------------------------------------
    std::shared_ptr<fastLEC::XAG>
    extract_sub_graph(const std::vector<int> vec_po);

    //---------------------------------------------------
    // format conversion
    //---------------------------------------------------
    void construct_from_aig(const fastLEC::AIG &aig);

    std::unique_ptr<fastLEC::CNF> construct_cnf_from_this_xag();

    std::vector<int> lmap_xag_to_cnf;
    int to_cnf_var(int xag_var);
    int to_cnf_lit(int xag_lit);
};

} // namespace fastLEC

std::ostream &operator<<(std::ostream &os, const fastLEC::Gate &gate);
std::ostream &operator<<(std::ostream &os, const fastLEC::XAG &xag);