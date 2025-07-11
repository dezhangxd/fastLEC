#include "fastLEC.hpp"
#include "parser.hpp"
#include "basic.hpp"

using namespace fastLEC;

bool Prover::read_aiger(const std::string &filename)
{
    std::string file = filename;
    if (file.empty())
        file = Param::get().input_file;

    if (Param::get().verbose > 0)
        printf("c [Prover] Start to parsing AIGER file: %s\n", file.c_str());

    // 创建新的AIG对象
    aig = std::make_unique<fastLEC::AIG>();
    if (!aig->construct(file)) {
        fprintf(stderr, "c [Prover] Failed to construct AIG from file: %s\n", file.c_str());
        aig.reset();
        return false;
    }

    if (Param::get().verbose > 0)
        printf("c [Prover] Successfully loaded AIGER file: %s\n", file.c_str());

    return true;
}

bool Prover::construct_XAG_from_AIG()
{
    if (!has_aig()) {
        fprintf(stderr, "c [fastLEC] Error: AIG not constructed yet\n");
        return false;
    }

    if (Param::get().verbose > 0)
        printf("c [fastLEC] Constructing XAG from AIG...\n");

    // 创建新的XAG对象
    xag = std::make_unique<fastLEC::XAG>();
    
    // TODO: 实现从AIG到XAG的转换逻辑
    // xag->construct_from_aig(*aig);
    
    if (Param::get().verbose > 0)
        printf("c [fastLEC] Successfully constructed XAG\n");

    return true;
}

bool Prover::construct_CNF_from_XAG()
{
    if (!has_xag()) {
        fprintf(stderr, "c [fastLEC] Error: XAG not constructed yet\n");
        return false;
    }

    if (Param::get().verbose > 0)
        printf("c [fastLEC] Constructing CNF from XAG...\n");

    // 创建新的CNF对象
    cnf = std::make_unique<fastLEC::CNF>();
    
    // TODO: 实现从XAG到CNF的转换逻辑
    // cnf->construct_from_xag(*xag);
    
    if (Param::get().verbose > 0)
        printf("c [fastLEC] Successfully constructed CNF\n");

    return true;
}

fastLEC::ret_vals Prover::check_cec()
{
    // if (!has_cnf()) {
    //     fprintf(stderr, "c [Prover] Error: CNF not constructed yet\n");
    //     return fastLEC::ret_vals::ret_UNK;
    // }

    if (Param::get().verbose > 0)
        printf("c [Prover] Starting CEC check...\n");

    // TODO: 实现CEC检查逻辑
    // cnf->solve();
    
    if (Param::get().verbose > 0)
        printf("c [Prover] CEC check completed\n");

    return fastLEC::ret_vals::ret_UNK;
}