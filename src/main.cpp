#include "fastLEC.hpp"
#include "parser.hpp"
#include <iostream>

int main(int argc, char *argv[])
{
    try
    {
        if (!fastLEC::Param::get().parse(argc, argv))
        {
            return 1;
        }

        if (fastLEC::Param::get().custom_params.log_features ||
            fastLEC::Param::get().custom_params.log_sub_aiger ||
            fastLEC::Param::get().custom_params.log_sub_cnfs)
        {
            fastLEC::Param::get().mode = fastLEC::Mode::SAT_sweeping;
        }

        fastLEC::Prover prover;

        bool ret = prover.read_aiger();
        if (!ret)
        {
            std::cout << "c [Main] Failed to build AIGER netlist" << std::endl;
            return 1;
        }

        fastLEC::ret_vals ret_val = prover.check_cec();

        if (ret_val == fastLEC::ret_vals::ret_SAT)
        {
            std::cout << "s Not Equivalent." << std::endl;
        }
        else if (ret_val == fastLEC::ret_vals::ret_UNS)
        {
            std::cout << "s Equivalent." << std::endl;
        }
        else
        {
            std::cout << "s Unknown." << std::endl;
        }

        std::cout << "c Runtime: " << fastLEC::ResMgr::get().get_runtime()
                  << " seconds" << std::endl;

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unknown exception occurred" << std::endl;
        return 1;
    }
}