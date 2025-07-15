#include "fastLEC.hpp"
#include "parser.hpp"
#include "basic.hpp"

using namespace fastLEC;

bool Prove_Task::build_xag()
{
    if (this->has_aig())
    {
        this->xag = std::make_unique<fastLEC::XAG>(*this->aig);
        return true;
    }
    return false;
}

bool Prove_Task::build_cnf()
{
    if (this->has_xag())
    {
        this->cnf = this->xag->construct_cnf_from_this_xag();
        return true;
    }
    else
    {
        if (this->has_aig())
        {
            this->xag = std::make_unique<fastLEC::XAG>(*this->aig);
            this->cnf = this->xag->construct_cnf_from_this_xag();
            return true;
        }
        return false;
    }
}

bool Prover::read_aiger(const std::string &filename)
{
    double start_time = fastLEC::ResMgr::get().get_runtime();
    std::string file = filename;
    if (file.empty())
        file = Param::get().input_file;

    if (Param::get().verbose > 0)
        printf("c [Prover] Start to parsing AIGER file: %s\n", file.c_str());

    // Initialize main_task if not already done
    if (!main_task)
    {
        main_task = std::make_unique<fastLEC::Prove_Task>();
    }

    // Create AIG object
    auto aig = std::make_unique<fastLEC::AIG>();
    if (!aig->construct(file))
    {
        fprintf(stderr, "c [Prover] Failed to construct AIG from file: %s\n", file.c_str());
        return false;
    }

    // Store the AIG in main_task
    main_task->set_aig(std::move(aig));

    if (Param::get().verbose > 0)
        printf("c [Prover] read AIGER file in %f seconds\n", fastLEC::ResMgr::get().get_runtime() - start_time);

    return true;
}

fastLEC::ret_vals Prover::check_cec()
{
    double start_time = fastLEC::ResMgr::get().get_runtime();
    if (Param::get().verbose > 0)
        printf("c [CEC] Starting CEC check...\n");

    if (!main_task || !main_task->has_aig())
    {
        fprintf(stderr, "c [CEC] Error: AIG not properly initialized\n");
        return ret_vals::ret_UNK;
    }

    const auto &aig = main_task->get_aig();
    if (aig.get()->outputs[0].lit == 0)
    {
        printf("c [CEC] PO-const-0: netlist is unsatisfiable\n");
        return ret_vals::ret_UNS;
    }
    else if (aig.get()->outputs[0].lit == 1)
    {
        printf("c [CEC] PO-const-1: netlist is valid\n");
        return ret_vals::ret_SAT;
    }

    bool b_res = main_task->build_cnf();
    if(!b_res)
    {
        fprintf(stderr, "c [CEC] Error: Failed to build CNF\n");
        return ret_vals::ret_UNK;
    }

    ret_vals ret = ret_vals::ret_UNK;
    if(Param::get().mode == Mode::SAT)
    {
        ret = main_task->seq_sat_kissat();
    }
    else if(Param::get().mode == Mode::BDD)
    {
        ret = main_task->seq_bdd_cudd();
    }


    if (Param::get().verbose > 0)
        printf("c [CEC] CEC check completed in %f seconds\n", fastLEC::ResMgr::get().get_runtime() - start_time);

    return ret;
}