#include "fastLEC.hpp"
#include "parser.hpp"
#include <thread>
#include <chrono>
#include <functional>

using namespace fastLEC;

extern "C"
{
#include "../deps/kissat/src/kissat.h"
}

ret_vals fastLEC::Prover::seq_SAT_kissat(std::shared_ptr<fastLEC::CNF> cnf)
{
    if (!cnf)
    {
        fprintf(stderr, "c [CEC] Error: CNF not properly initialized\n");
        return ret_vals::ret_UNK;
    }

    double start_time = fastLEC::ResMgr::get().get_runtime();

    auto solver = std::shared_ptr<kissat>(kissat_init(), kissat_release);

    int start_pos = 0, end_pos = 0;
    for (int i = 0; i < cnf->num_clauses(); i++)
    {
        end_pos = cnf->cls_end_pos[i];
        for (int j = start_pos; j < end_pos; j++)
        {
            kissat_add(solver.get(), cnf->lits[j]);
        }
        kissat_add(solver.get(), 0);
        start_pos = end_pos;
    }

    for (int al : cnf->assumptions)
    {
        kissat_add(solver.get(), al);
        kissat_add(solver.get(), 0);
    }

    double time_resource =
        Param::get().timeout - fastLEC::ResMgr::get().get_runtime();
    if (Param::get().custom_params.log_sub_aiger ||
        Param::get().custom_params.log_sub_cnfs)
    {
        time_resource = 5.0;
    }

    std::function<void()> func = [solver, time_resource]()
    {
        const double check_interval = 0.05;
        auto start_time = std::chrono::high_resolution_clock::now();
        while (std::chrono::duration_cast<std::chrono::duration<double>>(
                   std::chrono::high_resolution_clock::now() - start_time)
                   .count() < time_resource)
        {
            std::this_thread::sleep_for(
                std::chrono::duration<double>(check_interval));
        }

        if (solver)
            kissat_terminate(solver.get());
    };

    std::thread t(func);
    t.detach();

    int ret = kissat_solve(solver.get());

    if (fastLEC::Param::get().verbose > 0)
    {
        printf("c [SAT] result = %d [var = %d, clause = %d, lit = %d] [time = "
               "%.2f]\n",
               ret,
               cnf->num_vars,
               cnf->num_clauses(),
               cnf->num_lits(),
               fastLEC::ResMgr::get().get_runtime() - start_time);
    }

    return ret_vals(ret);
}