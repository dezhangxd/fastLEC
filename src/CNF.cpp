#include "CNF.hpp"
#include <iomanip>

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
    // printf("add_a_variable %d -> num_vars %d\n", xag_var, this->num_vars + 1);
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