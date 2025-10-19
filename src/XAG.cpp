#include "XAG.hpp"
#include "AIG.hpp"
#include "parser.hpp"

extern "C"
{
#include "../deps/aiger/aiger.h"
}

#include <cassert>
#include <cstdio>
#include <climits>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <queue>

// #define PRT_DEBUG_XAG

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
        os << "(v";
        os << std::setw(8) << aiger_var(gate.inputs[0]);
        os << ")";
    }
    else if (gate.type == GateType::AND2 || gate.type == GateType::XOR2)
    {
        os << (gate.type == GateType::AND2 ? "AND2" : "XOR2") << " : ";
        os << std::setw(8) << gate.output;
        os << " <l- ";
        os << std::setw(8) << gate.inputs[0];
        os << (gate.type == GateType::AND2 ? " & " : " ^ ");
        os << std::setw(8) << gate.inputs[1];

        os << " ,      ";

        os << std::setw(8) << aiger_var(gate.output);
        os << " <v- ";
        os << std::setw(8) << aiger_var(gate.inputs[0]);
        os << (gate.type == GateType::AND2 ? " & " : " ^ ");
        os << std::setw(8) << aiger_var(gate.inputs[1]);
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

        // for (auto g : this->gates)
        // {
        //     printf("g: %c %d %d %d\n",
        //            g.type == (GateType::NUL || g.type == GateType::PI)
        //                ? '?'
        //                : (g.type == GateType::AND2 ? 'A' : 'X'),
        //            g.output,
        //            g.inputs[0],
        //            g.inputs[1]);
        //     fflush(stdout);
        // }

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

            // printf("g[%d]: %c %d %d %d\n",
            //        i,
            //        g.type == (GateType::NUL || g.type == GateType::PI)
            //            ? '?'
            //            : (g.type == GateType::AND2 ? 'A' : 'X'),
            //        g.output,
            //        g.inputs[0],
            //        g.inputs[1]);
            // fflush(stdout);

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

void XAG::init_var_replace()
{
    this->var_replace.clear();
    this->var_replace.resize(this->max_var + 1, -1);
    // for (unsigned i = 0; i < this->var_replace.size(); i++)
    for (unsigned l : this->PI)
    {
        int v = aiger_var(l);
        this->var_replace[v] = v;
    }
    for (unsigned i : this->used_gates)
        this->var_replace[i] = i;
}

std::shared_ptr<fastLEC::XAG> XAG::extract_sub_graph(std::vector<int> vec_po)
{
    if (vec_po.size() == 2 && vec_po[0] > vec_po[1])
        std::swap(vec_po[0], vec_po[1]);

    std::function<int(int)> find_father = [&](int v) -> int
    {
        if (this->var_replace[v] != v)
            this->var_replace[v] = find_father(this->var_replace[v]);

        return this->var_replace[v];
    };

    // ---------------------------------------------------
    // step1: extract the cones
    // ---------------------------------------------------
    std::vector<bool> refs(this->max_var + 1, false);
    int v0 = 0, v1 = 0;
    int pos = -1;
    for (unsigned i = 0; i < vec_po.size(); i++)
    {
        int &v = (i == 0) ? v0 : v1;
        v = find_father(aiger_var(vec_po[i]));
        pos = std::max(pos, v);
        refs[v] = true;
    }

    std::vector<fastLEC::Gate> tmp_gates;
    tmp_gates.clear();
    for (; pos >= 0; pos--)
    {
        if (!refs[pos])
            continue;

        Gate &g = this->gates[pos];

        if (g.type == fastLEC::GateType::AND2 ||
            g.type == fastLEC::GateType::XOR2)
        {
            int v_i0 = find_father(aiger_var(g.inputs[0]));
            int v_i1 = find_father(aiger_var(g.inputs[1]));
            int l_i0 = aiger_sign(g.inputs[0]) ? aiger_neg_lit(v_i0)
                                               : aiger_pos_lit(v_i0);
            int l_i1 = aiger_sign(g.inputs[1]) ? aiger_neg_lit(v_i1)
                                               : aiger_pos_lit(v_i1);
            tmp_gates.emplace_back(Gate(g.output, g.type, l_i0, l_i1));

            refs[v_i1] = true;
            refs[v_i0] = true;
        }
    }
    std::reverse(tmp_gates.begin(), tmp_gates.end());

    // ---------------------------------------------------
    // step 2: reduce const 0 1 variables
    // ---------------------------------------------------
    std::vector<int> mp(2 * (this->max_var + 1), 0);
    for (unsigned i = 0; i < mp.size(); i++)
        mp[i] = i;
    std::vector<bool> use_flag(this->max_var + 1, false);
    unsigned i = 0, j = 0;
    for (; i < tmp_gates.size(); i++)
    {
        Gate &g = tmp_gates[i];

        unsigned i0 = mp[g.inputs[0]];
        unsigned i1 = mp[g.inputs[1]];

        bool keep = false;
        if (g.type == GateType::AND2)
        {
            if (i0 == (i1 ^ 1))
                mp[g.output] = 0, mp[g.output ^ 1] = 1;
            else if (i0 == i1)
                mp[g.output] = mp[i0], mp[g.output ^ 1] = mp[i0 ^ 1];
            else if (i0 == 0 || i0 == 0)
                mp[g.output] = 0, mp[g.output ^ 1] = 1;
            else if (i0 == 1)
                mp[g.output] = mp[i1], mp[g.output ^ 1] = mp[i1 ^ 1];
            else if (i1 == 1)
                mp[g.output] = mp[i0], mp[g.output ^ 1] = mp[i0 ^ 1];
            else
                keep = true;
        }
        else if (g.type == GateType::XOR2)
        {
            if (i0 == (i1 ^ 1))
                mp[g.output] = 1, mp[g.output ^ 1] = 0;
            else if (i0 == i1)
                mp[g.output] = 0, mp[g.output ^ 1] = 1;
            else if (i0 == 0)
                mp[g.output] = mp[i1], mp[g.output ^ 1] = mp[i1 ^ 1];
            else if (i1 == 0)
                mp[g.output] = mp[i0], mp[g.output ^ 1] = mp[i0 ^ 1];
            else if (i0 == 1)
                mp[g.output] = mp[i1 ^ 1], mp[g.output ^ 1] = mp[i1];
            else if (i1 == 1)
                mp[g.output] = mp[i0 ^ 1], mp[g.output ^ 1] = mp[i0];
            else
                keep = true;
        }

        if (keep)
        {
            tmp_gates[j++] = g;
            use_flag[aiger_var(i0)] = true;
            use_flag[aiger_var(i1)] = true;
            use_flag[aiger_var(g.output)] = true;
        }
    }
    tmp_gates.resize(j);
    v0 = mp[aiger_pos_lit(v0)] >> 1;
    v1 = mp[aiger_pos_lit(v1)] >> 1;

    // ---------------------------------------------------
    // step 3: compact: removing useless variables
    // ---------------------------------------------------
    int mapper_cnt = 1; // variable counter
    // literal mapper, reordering the literals
    std::vector<int> mapper(2 * (this->max_var + 1), -1);
    mapper[0] = 0, mapper[1] = 1;
    for (int v = 1; v <= this->max_var; v++)
    {
        if (use_flag[v])
        {
            mapper[aiger_pos_lit(v)] = aiger_pos_lit(mapper_cnt);
            mapper[aiger_neg_lit(v)] = aiger_neg_lit(mapper_cnt);
            mapper_cnt++;
        }
    }

    // ---------------------------------------------------
    // step 4: build sub-XAG
    // ---------------------------------------------------
    std::shared_ptr<fastLEC::XAG> sub_xag = std::make_shared<fastLEC::XAG>();

    sub_xag->max_var = mapper_cnt;
    sub_xag->used_lits.resize(2 * (sub_xag->max_var + 2), false);
    sub_xag->PI.clear();
    sub_xag->used_gates.clear();
    sub_xag->gates.resize(sub_xag->max_var + 2);
    for (int l : this->PI)
    {
        int v_i = find_father(aiger_var(l));
        if (use_flag[v_i])
        {
            int pi_l = mapper[aiger_pos_lit(v_i)];
            sub_xag->PI.push_back(pi_l);
            sub_xag->gates[aiger_var(pi_l)] =
                fastLEC::Gate(pi_l, GateType::PI, 0, 0);
        }
    }
    sub_xag->num_PIs_org = sub_xag->PI.size();

    for (auto &g : tmp_gates)
    {
        int o = mapper[g.output];
        int i0 = mapper[g.inputs[0]];
        int i1 = mapper[g.inputs[1]];
        sub_xag->gates[aiger_var(o)] = fastLEC::Gate(o, g.type, i0, i1);
        sub_xag->used_gates.emplace_back(aiger_var(o));
    }

    if (vec_po.size() == 2)
    {
        int l0 = aiger_sign(vec_po[0]) ? aiger_neg_lit(v0) : aiger_pos_lit(v0);
        int l1 = aiger_sign(vec_po[1]) ? aiger_neg_lit(v1) : aiger_pos_lit(v1);
        // assert(mp[l0] != mp[l1]); // two cones should be useful
        l0 = mapper[l0], l1 = mapper[l1];
        sub_xag->max_var = mapper_cnt;
        sub_xag->PO = aiger_pos_lit(mapper_cnt);
        sub_xag->gates[mapper_cnt] = fastLEC::Gate(sub_xag->PO, XOR2, l0, l1);
        sub_xag->used_gates.emplace_back(mapper_cnt);
    }
    else if (vec_po.size() == 1)
    {
        if (v0 == 0 || v0 == 1) // constant
            sub_xag->PO = v0;
        else // variables
            sub_xag->PO = aiger_sign(vec_po[0]) ? mapper[aiger_neg_lit(v0)]
                                                : mapper[aiger_pos_lit(v0)];
    }
    else
    {
        printf("Error: vec_po.size() == %zu\n", vec_po.size());
        exit(1);
    }

    // ---------------------------------------------------
    // step 5: mark used literals
    // ---------------------------------------------------
    sub_xag->v_usr.clear();
    sub_xag->v_usr.resize(sub_xag->max_var + 1);
    sub_xag->used_lits[sub_xag->PO] = true;
    for (unsigned i = aiger_var(sub_xag->PO); i > sub_xag->PI.size(); i--)
    {
        const Gate &g = sub_xag->gates[i];
        int o = g.output;
        int i0 = g.inputs[0];
        int i1 = g.inputs[1];
        sub_xag->v_usr[aiger_var(i0)].push_back(aiger_var(o));
        sub_xag->v_usr[aiger_var(i1)].push_back(aiger_var(o));
        if (g.type == XOR2 || sub_xag->used_lits[o])
        {
            sub_xag->used_lits[i0] = true;
            sub_xag->used_lits[i1] = true;
        }

        if (g.type == XOR2 || sub_xag->used_lits[aiger_not(o)])
        {
            sub_xag->used_lits[aiger_not(i0)] = true;
            sub_xag->used_lits[aiger_not(i1)] = true;
        }
    }

    // ---------------------------------------------------
    // step 6: father son mapper
    // ---------------------------------------------------
    sub_xag->father_var_mapper.resize(this->max_var + 1, -1);
    sub_xag->son_var_mapper.resize(sub_xag->max_var + 1, -1);
    for (int v = 1; v <= this->max_var; v++)
    {
        int f_v = v;
        int s_l = mapper[aiger_pos_lit(v)];
        if (s_l == -1)
            continue;
        int s_v = aiger_var(s_l);
        sub_xag->father_var_mapper[f_v] = s_v;
        sub_xag->son_var_mapper[s_v] = f_v;
    }

    return sub_xag;
}

std::shared_ptr<fastLEC::AIG> XAG::construct_aig_from_this_xag()
{
    std::shared_ptr<fastLEC::AIG> aig = std::make_shared<fastLEC::AIG>();
    if (aig->get().get() != nullptr)
    {
        printf("c [XAG->AIG] warning: this aiger is already initialized\n");
        aig->get().reset();
    }
    aig->get() = aig->create();
    if (aig->get().get() == nullptr)
    {
        printf("c [XAG->AIG] Failed to initialize AIGER\n");
        return nullptr;
    }

    this->mp_xag_to_aig_var.resize(this->max_var + 1, 0);

    for (int i : this->PI)
    {
        aiger_add_input(aig->get().get(), i, NULL);
        this->mp_xag_to_aig_var[aiger_var(i)] = aiger_var(i);
    }

    int cnt = this->PI.size() + 1;
    for (int i : this->used_gates)
    {
        const Gate &g = this->gates[i];
        if (Param::get().verbose > 3)
        {
            printf("g: %c %d %d %d\n",
                   g.type == (GateType::NUL || g.type == GateType::PI)
                       ? '?'
                       : (g.type == GateType::AND2 ? 'A' : 'X'),
                   g.output,
                   g.inputs[0],
                   g.inputs[1]);
            fflush(stdout);
        }

        if (g.type == GateType::AND2)
        {
            this->mp_xag_to_aig_var[aiger_var(g.output)] = cnt;
            int v0 = mp_xag_to_aig_var[aiger_var(g.inputs[0])];
            int v1 = mp_xag_to_aig_var[aiger_var(g.inputs[1])];
            int l0 =
                aiger_sign(g.inputs[0]) ? aiger_neg_lit(v0) : aiger_pos_lit(v0);
            int l1 =
                aiger_sign(g.inputs[1]) ? aiger_neg_lit(v1) : aiger_pos_lit(v1);
            aiger_add_and(aig->get().get(), aiger_pos_lit(cnt), l0, l1);
            if (Param::get().verbose > 3)
            {
                printf(
                    "c [XAG->AIG] AND: %d %d %d\n", aiger_pos_lit(cnt), l0, l1);
                fflush(stdout);
            }
            cnt++;
        }
        else if (g.type == GateType::XOR2)
        {
            this->mp_xag_to_aig_var[aiger_var(g.output)] = cnt + 2;
            int v0 = mp_xag_to_aig_var[aiger_var(g.inputs[0])];
            int v1 = mp_xag_to_aig_var[aiger_var(g.inputs[1])];
            int l0 =
                aiger_sign(g.inputs[0]) ? aiger_neg_lit(v0) : aiger_pos_lit(v0);
            int l1 =
                aiger_sign(g.inputs[1]) ? aiger_neg_lit(v1) : aiger_pos_lit(v1);
            int c = aiger_pos_lit(cnt);
            int d = aiger_pos_lit(cnt + 1);
            int o = aiger_pos_lit(cnt + 2);
            aiger_add_and(aig->get().get(), c, l0, l1);
            aiger_add_and(aig->get().get(), d, aiger_not(l0), aiger_not(l1));
            aiger_add_and(aig->get().get(), o, aiger_not(c), aiger_not(d));
            if (Param::get().verbose > 3)
            {
                printf("c [XAG->AIG] AND: %d %d %d\n", c, l0, l1);
                printf("c [XAG->AIG] AND: %d %d %d\n",
                       d,
                       aiger_not(l0),
                       aiger_not(l1));
                printf("c [XAG->AIG] AND: %d %d %d\n",
                       o,
                       aiger_not(c),
                       aiger_not(d));
                fflush(stdout);
            }
            cnt += 3;
        }
        else
        {
            printf("Error: gate type is not AND2 or XOR2\n");
            exit(1);
        }
    }

    int o_v = this->mp_xag_to_aig_var[aiger_var(this->PO)];
    int o_l = aiger_sign(this->PO) ? aiger_neg_lit(o_v) : aiger_pos_lit(o_v);
    aiger_add_output(aig->get().get(), o_l, 0);

    if (Param::get().verbose > 2)
    {

        printf("c [XAG->AIG] MILOA: %u %u %u %u %u\n",
               aig->get()->maxvar,
               aig->get()->num_inputs,
               aig->get()->num_latches,
               aig->get()->num_outputs,
               aig->get()->num_ands);
        fflush(stdout);
    }

    return aig;
}

void fastLEC::XAG::compute_distance(const std::vector<bool> &mask,
                                    std::vector<int> &in_degree,
                                    std::vector<int> &out_degree,
                                    std::vector<int> &idis,
                                    std::vector<int> &odis)
{
    idis.resize(this->max_var + 1, INT_MAX);
    odis.resize(this->max_var + 1, INT_MAX);

    in_degree.resize(this->max_var + 1, 0);
    out_degree.resize(this->max_var + 1, 0);

    for (int v = 1; v <= this->max_var; v++)
    {
        if (mask[v])
            continue;

        const Gate &g = this->gates[v];
        if (g.type == GateType::AND2 || g.type == GateType::XOR2)
        {
            int i1 = aiger_var(g.inputs[0]);
            int i2 = aiger_var(g.inputs[1]);
            if (!mask[i1])
            {
                in_degree[v]++;
                out_degree[i1]++;
            }
            if (!mask[i2])
            {
                in_degree[v]++;
                out_degree[i2]++;
            }
        }
    }

    for (int v = 1; v <= this->max_var; v++)
    {
        if (!mask[v] && in_degree[v] == 0)
            idis[v] = 0;
    }

    for (int v = 1; v <= this->max_var; v++)
    {
        if (mask[v])
            continue;

        const Gate &g = this->gates[v];
        if (g.type == GateType::AND2 || g.type == GateType::XOR2)
        {
            int i1 = aiger_var(g.inputs[0]);
            int i2 = aiger_var(g.inputs[1]);
            if (!mask[i1] && !mask[i2] && idis[i1] != INT_MAX &&
                idis[i2] != INT_MAX)
                idis[v] = std::max(idis[i1], idis[i2]) + 1;
            else if (idis[i1] != INT_MAX && idis[i2] == INT_MAX)
                idis[v] = idis[i1]; // this gate becomes eql
            else if (idis[i1] == INT_MAX && idis[i2] != INT_MAX)
                idis[v] = idis[i2]; // this gate becomes eql
            else
                idis[v] = INT_MAX;
        }
    }

    for (int v = 1; v <= this->max_var; v++)
    {
        if (!mask[v] && out_degree[v] == 0)
            odis[v] = 0;
    }

    for (int v = this->max_var; v >= 1; v--)
    {
        if (mask[v])
            continue;

        const Gate &g = this->gates[v];
        if (g.type == GateType::AND2 || g.type == GateType::XOR2)
        {
            int i1 = aiger_var(g.inputs[0]);
            int i2 = aiger_var(g.inputs[1]);
            if (!mask[i1] && odis[v] != INT_MAX)
                odis[i1] = std::min(odis[i1], odis[v] + 1);
            if (!mask[i2] && odis[v] != INT_MAX)
                odis[i2] = std::min(odis[i2], odis[v] + 1);
        }
    }

#ifdef PRT_DEBUG_XAG
    {
        printf("c [debug]in_out_degree: ");
        for (int i = 1; i <= this->max_var; i++)
            printf("%d:%d,%d ", i, in_degree[i], out_degree[i]);
        printf("\n");
        fflush(stdout);
        printf("c [debug] idis, odis: ");
        for (int i = 1; i <= this->max_var; i++)
            printf("%d: %d,%d ", i, idis[i], odis[i]);
        printf("\n");
        fflush(stdout);
    }
#endif
}

void fastLEC::XAG::compute_cut_points(const std::vector<int> &selected_vars,
                                      std::vector<int> &cut_points)
{
    if (selected_vars.empty())
        return;

    std::vector<bool> is_selected(this->max_var + 1, false);
    for (int v : selected_vars)
        is_selected[v] = true;

    int time = 0;
    std::vector<int> disc(this->max_var + 1, -1);
    std::vector<int> low(this->max_var + 1, -1);
    std::vector<int> parent(this->max_var + 1, -1);
    std::vector<bool> ap(this->max_var + 1, false);

    auto get_neighbors = [&](int u, std::vector<int> &neighbors)
    {
        neighbors.clear();

        // check inputs side
        {
            int i1 = aiger_var(gates[u].inputs[0]);
            int i2 = aiger_var(gates[u].inputs[1]);

            if (is_selected[i1])
                neighbors.push_back(i1);

            if (is_selected[i2])
                neighbors.push_back(i2);
        }

        // check output side
        for (int v : this->v_usr[u])
        {
            if (is_selected[v])
                neighbors.push_back(v);
        }
    };

    // DFS for articulation points (Tarjan's algorithm)
    std::function<void(int)> dfs = [&](int u)
    {
        int children = 0;
        disc[u] = low[u] = ++time;

        std::vector<int> neighbors;
        get_neighbors(u, neighbors);

        for (int v : neighbors)
        {
            if (disc[v] == -1)
            {
                children++;
                parent[v] = u;

                dfs(v);

                low[u] = std::min(low[u], low[v]);

                // Case 1: If u is root of DFS tree and has two or more children
                if (parent[u] == -1 && children > 1)
                    ap[u] = true;

                // Case 2: If u is not root and low value of one of its child >=
                // disc[u]
                if (parent[u] != -1 && low[v] >= disc[u])
                    ap[u] = true;
            }
            else if (v != parent[u])
                low[u] = std::min(low[u], disc[v]);
        }
    };

    for (int u : selected_vars)
        if (disc[u] == -1)
            dfs(u);

    for (int u : selected_vars)
        if (ap[u])
            cut_points.push_back(u);
}

void fastLEC::XAG::compute_v_usr()
{
    this->v_usr.clear();
    this->v_usr.resize(this->max_var + 1);
    for (auto gid : this->used_gates)
    {
        const Gate &g = this->gates[gid];
        this->v_usr[aiger_var(g.inputs[0])].push_back(gid);
        this->v_usr[aiger_var(g.inputs[1])].push_back(gid);
    }
}

void fastLEC::XAG::compute_XOR_blocks(std::vector<std::vector<int>> &XOR_blocks,
                                      std::vector<int> &XOR_block_indices)
{
    if (fastLEC::Param::get().verbose > 3)
    {
        printf("c [netlist] finding XOR chains\n");
        int cnt = 0;
        int ands = 0;
        for (auto &gate : this->gates)
            if (gate.type == GateType::XOR2)
                ++cnt;
            else
                ++ands;
        printf("c [netlist] found %d XOR gates and %d AND gates\n", cnt, ands);
    }

    std::vector<int> parent(this->max_var + 1);
    for (int i = 0; i <= this->max_var; ++i)
        parent[i] = i;

    std::function<int(int)> find = [&](int x) -> int
    {
        if (parent[x] != x)
            parent[x] = find(parent[x]);
        return parent[x];
    };

    auto unionSets = [&](int x, int y) -> void
    {
        int rootX = find(x);
        int rootY = find(y);
        if (rootX != rootY)
            parent[rootX] = rootY;
    };

    for (const auto &gate : this->gates)
    {
        if (gate.type == GateType::XOR2)
        {
            int o_v = aiger_var(gate.output);
            int i0_v = aiger_var(gate.inputs[0]);
            int i1_v = aiger_var(gate.inputs[1]);
            if (this->gates[i0_v].type != GateType::XOR2 ||
                this->gates[i1_v].type != GateType::XOR2)
                continue;
            unionSets(o_v, i0_v);
            unionSets(o_v, i1_v);
        }
    }

    XOR_blocks.clear();
    XOR_blocks.resize(this->max_var + 1, std::vector<int>());
    XOR_block_indices.resize(this->max_var + 1, -1);

    for (int i = 0; i <= this->max_var; ++i)
    {
        int groupId = find(i);
        if (XOR_block_indices[groupId] == -1)
        {
            XOR_block_indices[groupId] = XOR_blocks.size();
            XOR_blocks.emplace_back();
        }
        XOR_blocks[XOR_block_indices[groupId]].push_back(i);
    }

    int j = 0;
    for (int i = 0; i < (int)XOR_blocks.size(); ++i)
    {
        if (XOR_blocks[i].size() == 0)
            continue;
        else if (XOR_blocks[i].size() == 1)
        {
            XOR_block_indices[XOR_blocks[i][0]] =
                -1; // -2 means it is a single variable
            continue;
        }
        XOR_blocks[j] = XOR_blocks[i];
        for (unsigned id = 0; id < XOR_blocks[j].size(); ++id)
        {
            int v = XOR_blocks[j][id];
            XOR_blocks[j][id] = aiger_pos_lit(v);
            XOR_block_indices[v] = j;
        }
        j++;
    }
    XOR_blocks.resize(j);

    if (fastLEC::Param::get().verbose > 3)
    {
        for (int i = 0; i < (int)XOR_blocks.size(); ++i)
        {
            printf("XOR block %d [sz = %d]: ", i, (int)XOR_blocks[i].size());
            for (int v : XOR_blocks[i])
                printf("%d ", v);
            puts("");
        }
    }
}

void fastLEC::XAG::compute_XOR_chains(
    const std::vector<bool> &mask,
    std::vector<std::vector<int>> &XOR_chains,
    std::vector<std::vector<int>> &important_nodes)
{
#ifdef PRT_DEBUG_XAG
    bool b_res = check_XAG();
    if (!b_res)
    {
        printf("Error: XAG is not built correctly\n");
        exit(1);
    }
    else
    {
        printf("XAG is built correctly\n");
        fflush(stdout);
    }
#endif

    // Clear previous results
    XOR_chains.clear();
    important_nodes.clear();

    if (this->v_usr.empty())
        compute_v_usr();

    // Find connected components using BFS
    std::vector<bool> visited(this->max_var + 1, false);

    for (int v = 1; v <= this->max_var; v++)
    {
        if (mask[v] || visited[v])
            continue;

        // Start a new XOR chain
        std::vector<int> chain;
        std::queue<int> q;
        q.push(v);
        visited[v] = true;

        while (!q.empty())
        {
            int u = q.front();
            q.pop();
            chain.push_back(u);

            // Use nexts to find connected nodes
            for (int w : this->v_usr[u])
            {
                assert(w > 0);
                if (mask[w] || visited[w])
                    continue;

                // Check if the connection is through an XOR gate
                const Gate &g = gates[w];
                if (g.type == GateType::XOR2 && !mask[w] && !visited[w])
                {
                    visited[w] = true;
                    q.push(w);
                }
            }

            const Gate &g = gates[u];
            if (g.type == GateType::XOR2)
            {
                int v1 = aiger_var(g.inputs[0]);
                int v2 = aiger_var(g.inputs[1]);
                if (!mask[v1] && !visited[v1])
                {
                    visited[v1] = true;
                    q.push(v1);
                }
                if (!mask[v2] && !visited[v2])
                {
                    visited[v2] = true;
                    q.push(v2);
                }
            }
        }

        // Only add chains that contain at least one XOR gate
        bool has_xor = false;
        for (int u : chain)
        {
            if (gates[u].type == GateType::XOR2)
            {
                has_xor = true;
                break;
            }
        }

        if (has_xor)
        {
            XOR_chains.push_back(chain);

            // Find cut points in this chain
            std::vector<int> cut_points;
            compute_cut_points(chain, cut_points);
            important_nodes.push_back(cut_points);
        }
    }

#ifdef PRT_DEBUG_XAG
    {
        printf("c [debug] XOR_chains: %zu\n", XOR_chains.size());
        for (unsigned i = 0; i < XOR_chains.size(); i++)
        {
            printf("c [debug] XOR_chain[%d]: ", i);
            for (unsigned j = 0; j < XOR_chains[i].size(); j++)
            {
                printf("%d ", XOR_chains[i][j]);
            }
            printf("{ important_nodes[%zu]: ", important_nodes[i].size());
            for (unsigned j = 0; j < important_nodes[i].size(); j++)
            {
                printf("%d ", important_nodes[i][j]);
            }
            printf("}\n");
            fflush(stdout);
        }
    }
#endif
}

bool fastLEC::XAG::check_XAG()
{
    // Check max_var
    int computed_max_var = -1;
    for (const auto &gate : gates)
    {
        if (aiger_var(gate.output) > computed_max_var)
            computed_max_var = aiger_var(gate.output);
        for (int i = 0; i < 2; ++i)
        {
            if (aiger_var(gate.inputs[i]) > computed_max_var)
                computed_max_var = aiger_var(gate.inputs[i]);
        }
    }
    if (computed_max_var != max_var)
    {
        printf("[check_XAG] max_var check failed: max_var=%d, "
               "computed_max_var=%d\n",
               max_var,
               computed_max_var);
        return false;
    }

    // check gates indices
    for (int v = 0; v <= max_var; v++)
    {
        if (v != aiger_var(this->gates[v].output))
        {
            printf("[check_XAG] gates[%d] output mismatch: %d != %d\n",
                   v,
                   aiger_var(v),
                   this->gates[v].output);
            std::cout << gates[v] << std::endl;
            return false;
        }
    }

    // Check PI: collect all gate outputs marked as PI, convert to aiger_var
    std::vector<int> computed_PI;
    for (const auto &gate : gates)
    {
        if (gate.type == fastLEC::GateType::PI)
        {
            computed_PI.push_back(gate.output);
        }
    }
    std::sort(computed_PI.begin(), computed_PI.end());
    std::vector<int> sorted_PI = PI;
    std::sort(sorted_PI.begin(), sorted_PI.end());
    if (computed_PI != sorted_PI)
    {
        printf("[check_XAG] PI check failed.\nCurrent PI: ");
        for (auto v : PI)
            printf("%d ", v);
        printf("\nComputed PI: ");
        for (auto v : computed_PI)
            printf("%d ", v);
        printf("\n");
        return false;
    }

    // Compute used_lits and used_gates
    std::vector<int> computed_used_gates;
    std::vector<bool> computed_used_lits(2 * (max_var + 1), false); // mask

    std::queue<int> q;
    q.push(PO);
    computed_used_lits[PO] = true;

    while (!q.empty())
    {
        int lit = q.front();
        q.pop();

        const Gate &gate = gates[aiger_var(lit)];

        if (gate.type == fastLEC::GateType::XOR2 ||
            gate.type == fastLEC::GateType::AND2)
        {
            computed_used_gates.push_back(aiger_var(lit));

            if (!computed_used_lits[gate.inputs[0]] &&
                !computed_used_lits[aiger_not(gate.inputs[0])])
            {
                q.push(gate.inputs[0]);
            }

            if (!computed_used_lits[gate.inputs[1]] &&
                !computed_used_lits[aiger_not(gate.inputs[1])])
            {
                q.push(gate.inputs[1]);
            }

            if (computed_used_lits[aiger_not(gate.output)] ||
                gate.type == fastLEC::GateType::XOR2)
            {
                computed_used_lits[aiger_not(gate.inputs[0])] = true;
                computed_used_lits[aiger_not(gate.inputs[1])] = true;
            }

            if (computed_used_lits[gate.output] ||
                gate.type == fastLEC::GateType::XOR2)
            {
                computed_used_lits[(gate.inputs[0])] = true;
                computed_used_lits[(gate.inputs[1])] = true;
            }
        }
    }

    computed_PI.clear();
    {
        for (int i = 0; i < max_var + 1; i++)
        {
            if (gates[i].type == fastLEC::GateType::PI &&
                (computed_used_lits[aiger_pos_lit(i)] ||
                 computed_used_lits[aiger_neg_lit(i)]))
            {
                computed_PI.push_back(aiger_pos_lit(i));
            }
        }
    }

    for (int i = 1; i < max_var; i++)
    {
        if (!computed_used_lits[aiger_pos_lit(i)] &&
            !computed_used_lits[aiger_neg_lit(i)])
        {
            printf("Error: this gate is not used: %d\n", i);
        }
    }

    std::vector<int> sorted_computed_PI = computed_PI;
    std::vector<int> sorted_this_PI = PI;
    std::sort(sorted_computed_PI.begin(), sorted_computed_PI.end());
    std::sort(sorted_this_PI.begin(), sorted_this_PI.end());
    if (sorted_computed_PI != sorted_this_PI)
    {
        printf("[check_XAG] PI (after used_lits scan) mismatch.\nCurrent PI: ");
        for (auto v : PI)
            printf("%d ", v);
        printf("\nComputed  PI: ");
        for (auto v : computed_PI)
            printf("%d ", v);
        printf("\n");
        return false;
    }

    std::vector<int> sorted_used_gates = used_gates;
    std::sort(computed_used_gates.begin(), computed_used_gates.end());
    std::sort(sorted_used_gates.begin(), sorted_used_gates.end());
    if (computed_used_gates != sorted_used_gates)
    {
        printf("[check_XAG] used_gates check failed.\nCurrent used_gates: ");
        for (auto v : used_gates)
            printf("%d ", v);
        printf("\nComputed used_gates: ");
        for (auto v : computed_used_gates)
            printf("%d ", v);
        printf("\n");
        return false;
    }

    bool used_lits_mismatch = false;
    for (size_t i = 0; i < used_lits.size(); ++i)
    {
        if (used_lits[i] != computed_used_lits[i])
        {
            printf("[check_XAG] used_lits (mask) mismatch at literal %zu. "
                   "used_lits: "
                   "%d, computed: %d\n",
                   i,
                   used_lits[i] ? 1 : 0,
                   computed_used_lits[i] ? 1 : 0);
            used_lits_mismatch = true;
        }
    }
    if (used_lits_mismatch)
        return false;

    // Check v_usr (users list)
    // Build users by aiger variable
    int nvar = max_var + 1;
    std::vector<std::vector<int>> computed_v_usr(nvar);
    for (const auto &gate : gates)
    {
        int out_var = aiger_var(gate.output);
        for (int i = 0; i < 2; ++i)
        {
            int in_var = aiger_var(gate.inputs[i]);
            computed_v_usr[in_var].push_back(out_var);
        }
    }

    for (int i = 1; i <= this->max_var; ++i)
    {
        std::vector<int> a = v_usr[i];
        std::vector<int> b = computed_v_usr[i];
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        if (a != b)
        {
            printf("[check_XAG] v_usr mismatch at var %d. v_usr:", i);
            for (auto x : a)
                printf("%d ", x);
            printf(" computed:");
            for (auto x : b)
                printf("%d ", x);
            printf("\n");
            return false;
        }
    }
    return true;
}
