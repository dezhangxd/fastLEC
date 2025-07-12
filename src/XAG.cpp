#include "XAG.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace fastLEC;

std::ostream &operator<<(std::ostream &os, const fastLEC::Gate &gate)
{
    if (gate.type == GateType::NUL)
    {
        os << "---- : ";
    }
    else if (gate.type == GateType::PI)
    {
        os << "PI   : ";
        os << std::setw(8) << gate.output;
    }
    else if (gate.type == GateType::AND2)
    {
        os << "AND2 : ";
        os << std::setw(8) << gate.output;
        os << " <-- ";
        os << std::setw(8) << gate.inputs[0];
        os << " & ";
        os << std::setw(8) << gate.inputs[1];
    }
    else if (gate.type == GateType::XOR2)
    {
        os << "XOR2 : ";
        os << std::setw(8) << gate.output;
        os << " <-- ";
        os << std::setw(8) << gate.inputs[0];
        os << " ^ ";
        os << std::setw(8) << gate.inputs[1];
    }
    else
    {
        os << "Fault gate";
    }
    return os;
}

std::ostream &operator<<(std::ostream &os, const fastLEC::XAG &xag)
{
    os << "c nPI [" << std::setw(5) << std::right << xag.PI.size() << "] : ";
    for (auto i : xag.PI)
        os << std::setw(8) << std::left << i << " ";
    os << std::endl;

    for (int i = 0; i < xag.max_var + 1; i++)
    {
        if (xag.gates[i].type != GateType::NUL && xag.gates[i].type != GateType::PI)
        {
            os << "c        ";
            os << xag.gates[i] << std::endl;
        }
    }
    os << "c        PO   : " << xag.PO << std::endl;
    return os;
}

void XAG::construct_from_aig(const fastLEC::AIG &aig)
{
    if (!aig.get())
    {
        throw std::runtime_error("XAG::construct_from_aig: Invalid AIG object");
    }

    this->max_var = aig.get()->maxvar;
    this->PI.clear();
    this->PO = aig.get()->outputs[0].lit;
    this->used_lits.resize(2 * this->max_var, false);
    this->gates.resize(this->max_var + 1);

    if (this->PO == 0 || this->PO == 1)
        return;

    used_lits[this->PO] = true;
    unsigned pos = aig.get()->num_ands;
    while (pos--)
    {
        aiger_and *and_gate = aig.get()->ands + pos;
        unsigned lhs = and_gate->lhs;
        unsigned not_lhs = aiger_not(lhs);
        if (!used_lits[lhs] && !used_lits[not_lhs])
            continue;
        unsigned rhs0 = and_gate->rhs0;
        unsigned rhs1 = and_gate->rhs1;
        bool xor_gate = aig.is_xor(lhs, &rhs0, &rhs1);

        unsigned not_rhs0 = aiger_not(rhs0);
        unsigned not_rhs1 = aiger_not(rhs1);

        if (xor_gate || used_lits[lhs])
        {
            used_lits[rhs0] = true;
            used_lits[rhs1] = true;
        }
        if (xor_gate || used_lits[not_lhs])
        {
            used_lits[not_rhs0] = true;
            used_lits[not_rhs1] = true;
        }
    }

    for (unsigned i = 0; i < aig.get()->num_inputs; i++)
    {
        unsigned lit = aig.get()->inputs[i].lit;
        if (used_lits[lit] || used_lits[aiger_not(lit)])
            this->PI.emplace_back(aiger_strip(lit));
    }

    for (unsigned aig_v = 1; aig_v <= aig.get()->maxvar; aig_v++)
    {
        unsigned lit = aiger_pos_lit(aig_v);
        unsigned not_lit = aiger_not(lit);
        if (!used_lits[lit] && !used_lits[not_lit])
        {
            this->gates[aig_v].output = aiger_pos_lit(aig_v);
            continue;
        }

        aiger_and *and_gate = aig.is_and(lit);
        if (and_gate == 0)
        {
            this->gates[aig_v].type = GateType::PI;
            this->gates[aig_v].output = aiger_pos_lit(aig_v);
            continue;
        }

        unsigned lhs = and_gate->lhs;
        unsigned rhs0 = and_gate->rhs0;
        unsigned rhs1 = and_gate->rhs1;
        bool xor_gate = aig.is_xor(lhs, &rhs0, &rhs1);
        if (xor_gate)
            this->gates[aig_v].set(lhs, GateType::XOR2, rhs0, rhs1);
        else
            this->gates[aig_v].set(lhs, GateType::AND2, rhs0, rhs1);
    }

    std::cout << *this << std::endl;
}
