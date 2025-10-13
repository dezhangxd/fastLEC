#pragma once

#include <vector>
#include <iostream>

namespace fastLEC
{

class CNF
{
public:
    CNF() : num_vars(0), use_mapper(false) {};
    ~CNF() = default;

    int num_vars;
    std::vector<int> lits;
    std::vector<int> cls_end_pos; // end positions for each clauses in lits.
    std::vector<int> assumptions; // assumptions

    int num_clauses() const { return cls_end_pos.size(); }
    int num_lits() const { return lits.size(); }
    int num_assumptions() const { return assumptions.size(); }

    void add_clause(const std::vector<int> &clause);

    void add_assumption(int lit) { assumptions.push_back(lit); }

    void log_cnf(const std::string &filename);

    // ------------------------------------------------------------
    bool use_mapper;
    std::vector<int> vmap_cnf_to_xag;
    int to_xag_var(int cnf_var);
    int to_xag_lit(int cnf_lit);
    void add_a_variable(int xag_var = -1);
    // ------------------------------------------------------------

    bool build_watches();
    std::vector<int> unit_clauses;
    std::vector<int> propagate(const std::vector<int> &cubes);
    std::vector<std::vector<int>> pos_watches, neg_watches;
    inline int get_clause_begin(int i);
    inline int get_clause_end(int i);
};

} // namespace fastLEC

std::ostream &operator<<(std::ostream &os, const fastLEC::CNF &cnf);