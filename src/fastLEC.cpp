#include "fastLEC.hpp"
#include "parser.hpp"
#include "basic.hpp"
#include "pSAT.hpp"

#include <cstddef>
#include <cstdio>
#include <memory>

using namespace fastLEC;

fastLEC::ret_vals Prover::fast_aig_check(std::shared_ptr<fastLEC::AIG> aig)
{
    if (!aig)
    {
        fprintf(stderr, "c [CEC] Error: AIG not properly initialized\n");
        fflush(stderr);
        return ret_vals::ret_UNK;
    }

    if (aig->get()->outputs[0].lit == 0)
    {
        printf("c [CEC] PO-const-0: netlist is unsatisfiable\n");
        fflush(stdout);
        return ret_vals::ret_UNS;
    }
    else if (aig->get()->outputs[0].lit == 1)
    {
        printf("c [CEC] PO-const-1: netlist is valid\n");
        fflush(stdout);
        return ret_vals::ret_SAT;
    }
    return ret_vals::ret_UNK;
}

bool Prover::read_aiger(const std::string &filename)
{
    double start_time = fastLEC::ResMgr::get().get_runtime();
    std::string file = filename;
    if (file.empty())
        file = Param::get().input_file;
    else
        Param::get().input_file = file;

    size_t last_slash = file.find_last_of('/');
    std::string filename_only;
    if (last_slash == std::string::npos)
        filename_only = file;
    else
        filename_only = file.substr(last_slash + 1);
    size_t last_dot = filename_only.find_last_of('.');
    if (last_dot != std::string::npos)
        Param::get().filename = filename_only.substr(0, last_dot);
    else
        Param::get().filename = filename_only;

    if (Param::get().verbose > 0)
        printf("c [Prover] Start to parsing AIGER file: %s\n", file.c_str());

    // Create AIG object
    this->aig = std::make_shared<fastLEC::AIG>();
    if (!aig->construct(file))
    {
        fprintf(stderr,
                "c [Prover] Failed to construct AIG from file: %s\n",
                file.c_str());
        return false;
    }

    if (Param::get().verbose > 0)
    {
        printf("c [Prover] load AIGER file in %f seconds\n",
               fastLEC::ResMgr::get().get_runtime() - start_time);
        printf(
            "c ------------------------------------------------------------\n");
        fflush(stdout);
    }
    return true;
}

fastLEC::ret_vals Prover::check_cec()
{
    double start_time = fastLEC::ResMgr::get().get_runtime();
    if (Param::get().verbose > 0)
    {
        printf("c [CEC] Starting CEC check...\n");
        fflush(stdout);
    }

    ret_vals ret = ret_vals::ret_UNK;
    ret = fast_aig_check(aig);
    if (ret != ret_vals::ret_UNK)
        return ret;

    FormatManager fm;
    fm.set_aig(aig);

    if (Param::get().mode == Mode::BDD || Param::get().mode == Mode::ES ||
        Param::get().mode == Mode::pES || Param::get().mode == Mode::gpuES ||
        Param::get().mode == Mode::pBDD ||
        Param::get().mode == Mode::SAT_sweeping ||
        Param::get().mode == Mode::pSAT_sweeping)
    {
        bool b_res = fm.aig_to_xag();
        if (!b_res)
        {
            fprintf(stderr, "c [CEC] Error: Failed to build XAG\n");
            return ret_vals::ret_UNK;
        }
        std::shared_ptr<fastLEC::XAG> xag = fm.get_xag_shared();

        if (Param::get().mode >= Mode::SAT_sweeping)
        {
            std::shared_ptr<fastLEC::Sweeper> sweeper =
                std::make_shared<fastLEC::Sweeper>(xag);
            ret = run_sweeping(sweeper);
        }
        else
        {
            switch (Param::get().mode)
            {
            case Mode::BDD:
                ret = seq_BDD_cudd(xag);
                break;
            case Mode::pBDD:
                ret = para_BDD_sylvan(xag, Param::get().n_threads);
                break;
            case Mode::ES:
                ret = seq_ES(xag);
                break;
            case Mode::pES:
                ret = para_ES(xag, Param::get().n_threads);
                break;
            case Mode::gpuES:
                ret = gpu_ES(xag);
                break;
            default:
                fprintf(stderr, "c [CEC] Error: Invalid mode\n");
                return ret_vals::ret_UNK;
                break;
            }
        }
    }
    else if (Param::get().mode == Mode::SAT)
    {
        bool b_res = fm.aig_to_cnf();
        if (!b_res)
        {
            fprintf(stderr, "c [CEC] Error: Failed to build CNF\n");
            return ret_vals::ret_UNK;
        }
        std::shared_ptr<fastLEC::CNF> cnf = fm.get_cnf_shared();

        ret = seq_SAT_kissat(cnf);
    }

    if (Param::get().verbose > 0)
    {
        printf("c [CEC] CEC check completed in %f seconds\n",
               fastLEC::ResMgr::get().get_runtime() - start_time);
        printf(
            "c ------------------------------------------------------------\n");
        fflush(stdout);
    }
    return ret;
}

// FormatManager implementation
void FormatManager::set_aig(std::shared_ptr<fastLEC::AIG> aig_ptr)
{
    // Clear existing AIG if any
    clear_aig();

    // Assign the new AIG object
    aig = aig_ptr;
}

void FormatManager::set_xag(std::shared_ptr<fastLEC::XAG> xag_ptr)
{
    // Clear existing XAG if any
    clear_xag();

    // Assign the new XAG object
    xag = xag_ptr;
}

void FormatManager::set_cnf(std::shared_ptr<fastLEC::CNF> cnf_ptr)
{
    // Clear existing CNF if any
    clear_cnf();

    // Assign the new CNF object
    cnf = cnf_ptr;
}

bool FormatManager::aig_to_xag()
{
    if (!has_aig())
    {
        fprintf(stderr,
                "c [FormatManager] No AIG available for conversion to XAG\n");
        return false;
    }

    // Clear existing XAG if any
    clear_xag();

    // Create XAG from AIG
    xag = std::make_shared<fastLEC::XAG>(*aig);

    return true;
}

bool FormatManager::aig_to_cnf()
{
    if (!has_aig())
    {
        fprintf(stderr,
                "c [FormatManager] No AIG available for conversion to CNF\n");
        return false;
    }

    // First convert AIG to XAG, then XAG to CNF
    if (!aig_to_xag())
    {
        fprintf(stderr, "c [FormatManager] Failed to convert AIG to XAG\n");
        return false;
    }

    if (!xag_to_cnf())
    {
        fprintf(stderr, "c [FormatManager] Failed to convert XAG to CNF\n");
        return false;
    }

    return true;
}

bool FormatManager::xag_to_cnf()
{
    if (!has_xag())
    {
        fprintf(stderr,
                "c [FormatManager] No XAG available for conversion to CNF\n");
        return false;
    }

    // Clear existing CNF if any
    clear_cnf();

    // Create CNF from XAG
    cnf = xag->construct_cnf_from_this_xag();

    return true;
}

bool FormatManager::xag_to_aig()
{
    // Note: XAG to AIG conversion is not implemented yet
    // This would require reconstructing AIG from XAG structure
    fprintf(stderr,
            "c [FormatManager] XAG to AIG conversion not implemented yet\n");
    return false;
}

bool FormatManager::cnf_to_xag()
{
    // Note: CNF to XAG conversion is not implemented yet
    // This would require reconstructing XAG from CNF clauses
    fprintf(stderr,
            "c [FormatManager] CNF to XAG conversion not implemented yet\n");
    return false;
}

void FormatManager::clear_aig()
{
    if (aig)
    {
        aig.reset();
    }
}

void FormatManager::clear_xag()
{
    if (xag)
    {
        xag.reset();
    }
}

void FormatManager::clear_cnf()
{
    if (cnf)
    {
        cnf.reset();
    }
}

void FormatManager::clear_all()
{
    clear_aig();
    clear_xag();
    clear_cnf();
}

void FormatManager::print_status() const
{
    printf("c [FormatManager] Status:\n");
    printf("c   AIG: %s\n", has_aig() ? "Available" : "Not available");
    printf("c   XAG: %s\n", has_xag() ? "Available" : "Not available");
    printf("c   CNF: %s\n", has_cnf() ? "Available" : "Not available");

    if (has_aig())
    {
        const auto &aig_ptr = aig->get();
        printf("c   AIG Info: MILOA = %u %u %u %u %u\n",
               aig_ptr->maxvar,
               aig_ptr->num_inputs,
               aig_ptr->num_latches,
               aig_ptr->num_outputs,
               aig_ptr->num_ands);
    }

    if (has_xag())
    {
        printf("c   XAG Info: max_var=%d, num_PIs=%zu, num_gates=%zu\n",
               xag->max_var,
               xag->PI.size(),
               xag->gates.size());
    }

    if (has_cnf())
    {
        printf("c   CNF Info: num_vars=%d, num_clauses=%d, num_lits=%d\n",
               cnf->num_vars,
               cnf->num_clauses(),
               cnf->num_lits());
    }
}

std::string FormatManager::get_format_info() const
{
    std::string info = "FormatManager Status: ";

    if (has_aig())
        info += "AIG ";
    if (has_xag())
        info += "XAG ";
    if (has_cnf())
        info += "CNF ";

    if (!has_aig() && !has_xag() && !has_cnf())
        info += "Empty";

    return info;
}

bool FormatManager::load_aig_and_convert_to_xag(const std::string &filename)
{
    // Create AIG object and load from file
    auto aig_ptr = std::make_shared<fastLEC::AIG>();
    if (!aig_ptr->construct(filename))
    {
        fprintf(stderr,
                "c [FormatManager] Failed to load AIG from file: %s\n",
                filename.c_str());
        return false;
    }

    // Set the AIG and convert to XAG
    set_aig(aig_ptr);
    return aig_to_xag();
}

bool FormatManager::load_aig_and_convert_to_cnf(const std::string &filename)
{
    // Create AIG object and load from file
    auto aig_ptr = std::make_shared<fastLEC::AIG>();
    if (!aig_ptr->construct(filename))
    {
        fprintf(stderr,
                "c [FormatManager] Failed to load AIG from file: %s\n",
                filename.c_str());
        return false;
    }

    // Set the AIG and convert to CNF
    set_aig(aig_ptr);
    return aig_to_cnf();
}

fastLEC::ret_vals Prover::para_SAT_pSAT(std::shared_ptr<fastLEC::XAG> xag,
                                        int n_t)
{
    fastLEC::ret_vals ret = ret_vals::ret_UNK;

    class fastLEC::PartitionSAT ps(xag, static_cast<unsigned>(n_t));

    ret = ps.check();

    return ret;
}

fastLEC::ret_vals
Prover::run_sweeping(std::shared_ptr<fastLEC::Sweeper> sweeper)
{
    fastLEC::ret_vals ret = ret_vals::ret_UNK;

    ret = sweeper->logic_simulation();
    if (ret == ret_vals::ret_SAT)
        return ret;

    std::shared_ptr<fastLEC::XAG> sub_graph = nullptr;

    while ((sub_graph = sweeper->next_sub_graph()))
    {
        if (Param::get().verbose > 0)
        {
            printf("%s\n", sweeper->sub_graph_string.c_str());
            fflush(stdout);
        }

        // ret = fast_aig_check(sub_graph->construct_aig_from_this_xag());
        // if(ret != ret_vals::ret_UNK)
        //     continue;

        // std::shared_ptr<fastLEC::CNF> cnf =
        //     sub_graph->construct_cnf_from_this_xag();

        // ret = this->seq_SAT_kissat(cnf);
        ret = this->para_SAT_pSAT(sub_graph, Param::get().n_threads);

        sweeper->post_proof(ret);

        if (ret == ret_vals::ret_UNK &&
            !Param::get().custom_params.log_sub_aiger &&
            !Param::get().custom_params.log_sub_cnfs)
            break;
    }

    return ret;
}