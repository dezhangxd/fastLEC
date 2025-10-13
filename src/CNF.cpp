#include "CNF.hpp"
#include <iomanip>
#include <fstream>
#include "AIG.hpp"

using namespace fastLEC;

std::ostream &operator<<(std::ostream &os, const fastLEC::CNF &cnf)
{
    os << "c [CNF] num_vars=" << cnf.num_vars;

    os << ",  num_clauses=" << cnf.num_clauses();
    os << ",  num_lit=" << cnf.lits.size();
    os << std::endl;

    int start_pos = 0, end_pos = 0;
    for (int i = 0; i < cnf.num_clauses(); i++)
    {
        end_pos = cnf.cls_end_pos[i];
        os << "c [clause] ";
        os << std::setw(6) << std::right << i << ": ";
        for (int j = start_pos; j < end_pos; j++)
            os << std::setw(5) << std::right << cnf.lits[j] << " ";
        start_pos = end_pos;
        os << std::endl;
    }
    return os;
}

void fastLEC::CNF::log_cnf(const std::string &filename)
{
    std::ofstream file(filename);
    file << "p cnf " << num_vars << " " << num_clauses() << std::endl;
    int start_pos = 0, end_pos = 0;
    for (int i = 0; i < this->num_clauses(); i++)
    {
        end_pos = this->cls_end_pos[i];
        for (int j = start_pos; j < end_pos; j++)
            file << this->lits[j] << " ";
        start_pos = end_pos;
        file << "0" << std::endl;
    }
    file.close();
}

void fastLEC::CNF::add_clause(const std::vector<int> &clause)
{
    lits.insert(lits.end(), clause.begin(), clause.end());
    // for (auto lit : clause)
    // {
    //     printf("%d ", lit);
    //     fflush(stdout);
    // }
    // printf("\n");
    // fflush(stdout);
    cls_end_pos.push_back(lits.size());
}

void fastLEC::CNF::add_a_variable(int xag_var)
{
    // printf("add_a_variable %d -> num_vars %d\n", xag_var, this->num_vars +
    // 1);
    this->num_vars++;
    if (use_mapper)
        this->vmap_cnf_to_xag.push_back(xag_var);
}

int fastLEC::CNF::to_xag_var(int cnf_var)
{
    if (!use_mapper)
        return -1;
    return vmap_cnf_to_xag[cnf_var];
}

int fastLEC::CNF::to_xag_lit(int cnf_lit)
{
    if (!use_mapper || cnf_lit == 0)
        return -1;
    if (cnf_lit > 0)
    {
        return to_xag_var(cnf_lit);
    }
    else
    {
        return aiger_neg_lit(to_xag_var(-cnf_lit));
    }
}

inline int fastLEC::CNF::get_clause_begin(int i)
{
    return (i == 0) ? 0 : cls_end_pos[i - 1];
}

inline int fastLEC::CNF::get_clause_end(int i) { return cls_end_pos[i]; }

bool fastLEC::CNF::build_watches()
{
    pos_watches.resize((num_vars + 1));
    neg_watches.resize((num_vars + 1));

    for (int i = 0; i < num_clauses(); i++)
    {
        int begin = get_clause_begin(i);
        int end = get_clause_end(i);
        if (end - begin == 0)
        {
            return false;
        }
        else if (end - begin == 1)
        {
            unit_clauses.push_back(lits[begin]);
        }
        for (int j = begin; j < end; j++)
        {
            int lit = lits[j];
            if (lit > 0)
                pos_watches[lit].push_back(i);
            else
                neg_watches[-lit].push_back(i);
        }
    }

    return true;
}

std::vector<int> fastLEC::CNF::propagate(const std::vector<int> &cubes)
{
    std::vector<int> propagated_cubes;

    std::vector<bool> assigned(num_vars + 1, false);
    std::vector<bool> value(num_vars + 1, false);

    std::function<bool(std::vector<int> &)> perform_bcp =
        [&](std::vector<int> &q) -> bool
    {
        unsigned q_pos = 0;
        while (q_pos < q.size())
        {
            int v = abs(q[q_pos++]);

            auto &ws = value[v] ? neg_watches[v] : pos_watches[v];

            for (int cls_idx : ws)
            {
                int cls_begin = get_clause_begin(cls_idx);
                int cls_end = get_clause_end(cls_idx);
                int unassigned_lit = -1;
                int unassigned_lit_cnt = 0;
                bool has_sat_lit = false;
                for (int i = cls_begin; i < cls_end; i++)
                {
                    int lit = lits[i];
                    if (assigned[abs(lit)])
                    {
                        if (value[abs(lit)] == (lit > 0))
                        {
                            has_sat_lit = true;
                            break;
                        }
                    }
                    else
                    {
                        unassigned_lit = lit;
                        unassigned_lit_cnt++;
                    }
                }

                if (has_sat_lit)
                {
                    continue;
                }
                else if (unassigned_lit_cnt == 0)
                {
                    return false;
                }
                else if (unassigned_lit_cnt == 1)
                {
                    int lit = unassigned_lit;
                    if (!assigned[abs(lit)])
                    {
                        q.emplace_back(lit);
                        assigned[abs(lit)] = true;
                        value[abs(lit)] = (lit > 0);
                    }
                }
            }
        }
        return true;
    };

    std::vector<int> q = unit_clauses;
    for (int lit : cubes)
        q.emplace_back(lit);

    for (int lit : q)
    {
        int v = abs(lit);
        bool val = (lit > 0);
        if (assigned[v])
        {
            if (value[v] != val)
            {
                return std::vector<int>({0});
            }
        }
        else
        {
            assigned[abs(lit)] = true;
            value[abs(lit)] = (lit > 0);
        }
    }

    if (perform_bcp(q))
    {
        return q;
    }
    else
    {
        return std::vector<int>({0});
    }
}