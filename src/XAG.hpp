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

        Gate(int o, fastLEC::GateType type = fastLEC::GateType::NUL, int i0 = -1, int i1 = -1) : output(o), type(type), inputs{i0, i1} {}

        int output;
        fastLEC::GateType type;
        int inputs[2];

        void set(int o, fastLEC::GateType type = fastLEC::GateType::NUL, int i0 = -1, int i1 = -1)
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

        int max_var;                         // the max variable number in the XAG
        int num_PIs_org;                     // the number of PIs in the original AIG
        std::vector<int> PI;                 // the used PIs
        int PO;                              // the PO (or const 0, 1)
        std::vector<Gate> gates;             // the gates (only considering AND and XOR)
        std::vector<int> used_gates;         // the used literals in the XAG
        std::vector<std::vector<int>> v_usr; // users (aiger variables) of a xag variable (not consider mediate var)
        std::vector<bool> used_lits;         // the used literals in the XAG
        
        void construct_from_aig(const fastLEC::AIG &aig);
        

        std::unique_ptr<fastLEC::CNF> construct_cnf_from_this_xag();
        
        std::vector<int> lmap_xag_to_cnf;
        int to_cnf_var(int xag_var);
        int to_cnf_lit(int xag_lit);

    };

}

std::ostream &operator<<(std::ostream &os, const fastLEC::Gate &gate);
std::ostream &operator<<(std::ostream &os, const fastLEC::XAG &xag);