#include "XAG.hpp"
#include "parser.hpp"

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

    for (int i : xag.used_gates)
    {
        const Gate &g = xag.gates[i];
        if (g.type != GateType::NUL && g.type != GateType::PI)
        {
            os << "c        ";
            os << g << std::endl;
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
    this->num_PIs_org = aig.get()->num_inputs;
    this->PI.clear();
    this->used_gates.clear();
    this->PO = aig.get()->outputs[0].lit;
    this->used_lits.resize(2 * (this->max_var + 1), false);

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
        this->used_gates.emplace_back(aig_v);
    }

    if (fastLEC::Param::get().verbose > 2)
        std::cout << *this << std::endl;
}

std::unique_ptr<fastLEC::CNF> XAG::construct_cnf_from_this_xag()
{
    std::unique_ptr<fastLEC::CNF> cnf = std::make_unique<fastLEC::CNF>();

    cnf->num_vars = 0;
    cnf->use_mapper = true;
    cnf->vmap_cnf_to_xag.clear();

    if (this->PO == 0)
    {
        cnf->num_vars = this->num_PIs_org;
        cnf->add_clause({});
    }
    else if (this->PO == 1)
    {
        cnf->num_vars = this->num_PIs_org;
    }
    else
    {
        if (used_lits.empty())
        {
            printf("Error: XAG is not built correctly, used_lits is empty\n");
            exit(1);
        }

        this->lmap_xag_to_cnf.resize(2 * (this->max_var + 1));

        if (used_lits[0] || used_lits[1])
        {
            lmap_xag_to_cnf[0] = -1;
            lmap_xag_to_cnf[1] = 1;
            cnf->add_a_variable(1);
            cnf->add_clause({1});
        }

        for (int i : PI)
        {
            cnf->add_a_variable(aiger_var(i));
            lmap_xag_to_cnf[i] = cnf->num_vars;
            lmap_xag_to_cnf[aiger_not(i)] = -cnf->num_vars;
        }

        for (int i : used_gates)
        {
            const Gate &g = gates[i];
            unsigned lhs = g.output;
            unsigned not_lhs = aiger_not(lhs);

            cnf->add_a_variable(aiger_var(lhs));
            lmap_xag_to_cnf[lhs] = cnf->num_vars;
            lmap_xag_to_cnf[not_lhs] = -cnf->num_vars;

            int o = cnf->num_vars;
            int i0 = to_cnf_lit(g.inputs[0]);
            int i1 = to_cnf_lit(g.inputs[1]);

            if (used_lits[lhs])
            {
                if (g.type == GateType::XOR2)
                {
                    cnf->add_clause({-o, i0, i1});
                    cnf->add_clause({-o, -i0, -i1});
                }
                else
                {
                    cnf->add_clause({-o, i0});
                    cnf->add_clause({-o, i1});
                }
            }
            if (used_lits[not_lhs])
            {
                if (g.type == GateType::XOR2)
                {
                    cnf->add_clause({o, i0, -i1});
                    cnf->add_clause({o, -i0, i1});
                }
                else
                {
                    cnf->add_clause({o, -i0, -i1});
                }
            }
        }

        cnf->add_clause({to_cnf_lit(this->PO)});
    }

    if (fastLEC::Param::get().verbose > 2)
        std::cout << *cnf << std::endl;

    return cnf;
}

int fastLEC::XAG::to_cnf_var(int xag_var)
{
    return lmap_xag_to_cnf[aiger_pos_lit(xag_var)];
}

int fastLEC::XAG::to_cnf_lit(int xag_lit) { return lmap_xag_to_cnf[xag_lit]; }

void fastLEC::XAG::topological_sort()
{
    this->topo_idx.clear();
    this->topo_idx.resize(this->max_var + 1);
    this->v_usr.clear();
    this->v_usr.resize(this->max_var + 1);

    std::vector<int> counter(this->max_var + 1, 0);

    for (auto gid : this->used_gates)
    {
        const Gate &g = this->gates[gid];
        assert(gid == aiger_var(g.output));
        int out_var = gid;
        int rhs0_var = aiger_var(g.inputs[0]);
        int rhs1_var = aiger_var(g.inputs[1]);

        if (rhs0_var == rhs1_var)
        {
            this->v_usr[rhs0_var].push_back(out_var);
            counter[out_var] = 1;
        }
        else
        {
            this->v_usr[rhs0_var].push_back(out_var);
            this->v_usr[rhs1_var].push_back(out_var);
            counter[out_var] = 2;
        }
    }

    unsigned topo_cnt = 0;
    std::queue<int> q;

    for (unsigned i = 0; i < this->PI.size(); i++)
    {
        int v = aiger_var(this->PI[i]);
        q.push(v);
        topo_idx[v] = ++topo_cnt;
    }

    while (!q.empty())
    {
        int v = q.front();
        q.pop();
        for (int next : this->v_usr[v])
        {
            if (--counter[next] == 0)
            {
                q.push(next);
                topo_idx[next] = ++topo_cnt;
            }
        }
    }

    if (fastLEC::Param::get().verbose > 3)
    {
        printf("used gates %zu \n", this->used_gates.size());
        printf("PI %zu \n", this->PI.size());
        printf("PO var %d \n", aiger_var(this->PO));
        for (unsigned i = 1; i <= (unsigned)this->max_var; i++)
        {
            printf("%5d:%5d \t", i, topo_idx[i]);
            if (i % 10 == 0)
                printf("\n");
        }
        printf("\n");
        fflush(stdout);
    }
}

void XAG::fast_compute_varcone_sizes()
{
    this->varcone_sizes.clear();
    this->varcone_sizes.resize(this->max_var + 1, 0);
    std::vector<int> queue(this->max_var + 1);
    std::vector<bool> visited(this->max_var + 1, false);
    for (unsigned i = 1; i <= (unsigned)this->max_var; i++)
    {
        unsigned left = 1, right = 0;
        for (unsigned j = 1; j <= (unsigned)this->max_var; j++)
            visited[j] = false;

        queue[++right] = i;
        visited[i] = true;
        while (left <= right)
        {
            int aig_v = queue[left++];
            Gate &g = this->gates[aig_v];
            if (g.type == GateType::AND2 || g.type == GateType::XOR2)
            {
                int i0 = aiger_var(g.inputs[0]);
                int i1 = aiger_var(g.inputs[1]);
                if (!visited[i0])
                {
                    queue[++right] = i0;
                    visited[i0] = true;
                }
                if (!visited[i1])
                {
                    queue[++right] = i1;
                    visited[i1] = true;
                }
            }
        }
        varcone_sizes[i] = right;
    }

    if (fastLEC::Param::get().verbose > 3)
    {
        for (int i = 1; i <= this->max_var; i++)
        {
            printf("%d:%d  \t", i, varcone_sizes[i]);
            if (i % 20 == 0)
                printf("\n");
        }
        printf("\n");
        fflush(stdout);
    }
}

bool XAG::strash_prune(unsigned n1, unsigned n2)
{
    unsigned n1v = aiger_var(n1);
    unsigned n2v = aiger_var(n2);

    if (varcone_sizes[n1v] != varcone_sizes[n2v])
        return false;

    std::vector<bool> visited1(this->max_var + 1, false);
    std::vector<bool> visited2(this->max_var + 1, false);
    std::queue<int> queue1, queue2;

    queue1.push(n1v);
    queue2.push(n2v);
    visited1[n1v] = true;
    visited2[n2v] = true;

    while (!queue1.empty() && !queue2.empty())
    {
        int v1 = queue1.front();
        int v2 = queue2.front();
        queue1.pop();
        queue2.pop();

        Gate &g1 = this->gates[v1];
        Gate &g2 = this->gates[v2];

        if (g1.type != g2.type)
            return false;

        if (g1.type != GateType::AND2 && g1.type != GateType::XOR2)
            continue;

        unsigned g1_l1 = g1.inputs[0];
        unsigned g1_l2 = g1.inputs[1];
        unsigned g2_l1 = g2.inputs[0];
        unsigned g2_l2 = g2.inputs[1];

        unsigned g1_v1 = aiger_var(g1_l1);
        unsigned g1_v2 = aiger_var(g1_l2);
        unsigned g2_v1 = aiger_var(g2_l1);
        unsigned g2_v2 = aiger_var(g2_l2);

        if (varcone_sizes[g1_v1] != varcone_sizes[g2_v1])
            std::swap(g2_v1, g2_v2), std::swap(g2_l1, g2_l1);
        if (varcone_sizes[g1_v1] != varcone_sizes[g2_v1] ||
            varcone_sizes[g1_v2] != varcone_sizes[g2_v2])
            return false;
        if (varcone_sizes[g1_v2] == varcone_sizes[g2_v2])
        {
            if (aiger_sign(g1_l1) != aiger_sign(g2_l1))
                std::swap(g2_v1, g2_v2), std::swap(g2_l1, g2_l1);

            if (visited1[g1_v1] != visited2[g2_v1])
                std::swap(g2_v1, g2_v2), std::swap(g2_l1, g2_l1);
        }
        if (aiger_sign(g1_l1) != aiger_sign(g2_l1) ||
            aiger_sign(g1_l2) != aiger_sign(g2_l2))
            return false;

        if (visited1[g1_v1] != visited2[g2_v1] ||
            visited1[g1_v2] != visited2[g2_v2])
            return false;

        if (!visited1[g1_v1])
        {
            queue1.push(g1_v1);
            queue2.push(g2_v1);
            visited1[g1_v1] = true;
            visited2[g2_v1] = true;
        }

        if (!visited1[g1_v2])
        {
            queue1.push(g1_v2);
            queue2.push(g2_v2);
            visited1[g1_v2] = true;
            visited2[g2_v2] = true;
        }
    }
    if (!queue1.empty() || !queue2.empty())
        return false;

    return true;
}

std::shared_ptr<fastLEC::XAG>
XAG::extract_sub_graph(const std::vector<int> vec_po)
{
    return std::make_shared<fastLEC::XAG>(*this);
}