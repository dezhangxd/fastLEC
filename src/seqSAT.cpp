#include "fastLEC.hpp"
#include "parser.hpp"
#include <thread>
#include <chrono>
#include <functional>

using namespace fastLEC;

extern "C"
{
#include "kissat.h"
}

ret_vals fastLEC::Prove_Task::seq_sat_kissat()
{
    if (!this->has_cnf())
    {
        fprintf(stderr, "c [CEC] Error: CNF not properly initialized\n");
        return ret_vals::ret_UNK;
    }

    double start_time = fastLEC::ResMgr::get().get_runtime();

    auto solver = std::unique_ptr<kissat, decltype(&kissat_release)>(kissat_init(), kissat_release);

    int start_pos = 0, end_pos = 0;
    for (int i = 0; i < this->cnf->num_clauses(); i++)
    {
        end_pos = this->cnf->cls_end_pos[i];
        for (int j = start_pos; j < end_pos; j++)
            kissat_add(solver.get(), this->cnf->lits[j]);
        kissat_add(solver.get(), 0);
        start_pos = end_pos;
    }

    double time_resource = Param::get().timeout - fastLEC::ResMgr::get().get_runtime();
    std::function<void()> func = [solver_ptr = solver.get(), time_resource]()
    {
        const double check_interval = 0.01;
        double elapsed = 0.0;
        while (elapsed < time_resource)
        {
            std::this_thread::sleep_for(std::chrono::duration<double>(check_interval));
            elapsed += check_interval;
        }

        if (solver_ptr != nullptr)
            kissat_terminate(solver_ptr);
    };

    std::thread t(func);
    t.detach();

    int ret = kissat_solve(solver.get());

    if (fastLEC::Param::get().verbose > 0)
    {
        printf("c [SAT] result = %d [var = %d, clause = %d] [time = %.2f]\n",
               ret, this->cnf->num_vars, this->cnf->num_clauses(), fastLEC::ResMgr::get().get_runtime() - start_time);
    }

    // Convert kissat result to our ret_vals
    if (ret == 10)  // SAT
        return ret_vals::ret_SAT;
    else if (ret == 20)  // UNSAT
        return ret_vals::ret_UNS;
    else  // UNKNOWN or other
        return ret_vals::ret_UNK;
}