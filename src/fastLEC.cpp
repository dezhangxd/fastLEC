#include "fastLEC.hpp"
#include "parser.hpp"
#include "basic.hpp"

using namespace fastLEC;

bool Prover::read_aiger(const std::string &filename)
{
    double start_time = fastLEC::ResMgr::get().get_runtime();
    std::string file = filename;
    if (file.empty())
        file = Param::get().input_file;

    if (Param::get().verbose > 0)
        printf("c [Prover] Start to parsing AIGER file: %s\n", file.c_str());

    aig = std::make_unique<fastLEC::AIG>();
    if (!aig->construct(file))
    {
        fprintf(stderr, "c [Prover] Failed to construct AIG from file: %s\n", file.c_str());
        aig.reset();
        return false;
    }

    if (Param::get().verbose > 0)
        printf("c [Prover] read AIGER file in %f seconds\n", fastLEC::ResMgr::get().get_runtime() - start_time);

    return true;
}


fastLEC::ret_vals Prover::check_cec()
{
    double start_time = fastLEC::ResMgr::get().get_runtime();
    if (Param::get().verbose > 0)
        printf("c [CEC] Starting CEC check...\n");

    if (!this->aig || !this->aig->get()) {
        fprintf(stderr, "c [CEC] Error: AIG not properly initialized\n");
        return ret_vals::ret_UNK;
    }

    if (this->aig->get()->outputs[0].lit == 0)
    {
        printf("c [CEC] PO-const-0: netlist is unsatisfiable\n");
        return ret_vals::ret_UNS;
    }
    else if (this->aig->get()->outputs[0].lit == 1)
    {
        printf("c [CEC] PO-const-1: netlist is valid\n");
        return ret_vals::ret_SAT;
    }

    this->xag = std::make_unique<fastLEC::XAG>(*this->aig);


    if (Param::get().verbose > 0)
        printf("c [CEC] CEC check completed in %f seconds\n", fastLEC::ResMgr::get().get_runtime() - start_time);

    return fastLEC::ret_vals::ret_UNK;
}