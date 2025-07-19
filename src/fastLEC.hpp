#pragma once

#include <string>
#include <memory>
#include "AIG.hpp"   // AIGER
#include "XAG.hpp"   // XAG
#include "CNF.hpp"   // CNF
#include "basic.hpp" // basic

namespace fastLEC
{

    class Prove_Task
    {
        std::unique_ptr<fastLEC::AIG> aig;
        std::unique_ptr<fastLEC::XAG> xag;
        std::unique_ptr<fastLEC::CNF> cnf;

    public:
        Prove_Task() = default;
        ~Prove_Task() = default;

        Prove_Task(const Prove_Task &) = delete;
        Prove_Task &operator=(const Prove_Task &) = delete;

        Prove_Task(Prove_Task &&) = default;
        Prove_Task &operator=(Prove_Task &&) = default;

        const fastLEC::AIG &get_aig() const { return *aig; }
        const fastLEC::XAG &get_xag() const { return *xag; }
        const fastLEC::CNF &get_cnf() const { return *cnf; }

        fastLEC::AIG &get_aig() { return *aig; }
        fastLEC::XAG &get_xag() { return *xag; }
        fastLEC::CNF &get_cnf() { return *cnf; }

        bool has_aig() const { return aig != nullptr; }
        bool has_xag() const { return xag != nullptr; }
        bool has_cnf() const { return cnf != nullptr; }

        // Setter methods
        void set_aig(std::unique_ptr<fastLEC::AIG> aig_ptr) { aig = std::move(aig_ptr); }
        void set_xag(std::unique_ptr<fastLEC::XAG> xag_ptr) { xag = std::move(xag_ptr); }
        void set_cnf(std::unique_ptr<fastLEC::CNF> cnf_ptr) { cnf = std::move(cnf_ptr); }

        // construct XAG or CNF from AIG
        bool build_xag();
        bool build_cnf();

        // CEC engine
        ret_vals seq_sat_kissat(); // call kissat
        ret_vals seq_es();         // call ES
        ret_vals seq_bdd_cudd();   // call cudd

        ret_vals para_es(int n_thread = 1); // call para-es engine
        ret_vals gpu_es();              // call gpu-es engine
    };

    class Prover
    {
        std::unique_ptr<fastLEC::Prove_Task> main_task;

    public:
        Prover() = default;
        ~Prover() = default;

        //---------------------------------------------------
        // read aiger filename from Param if filename = ""
        bool read_aiger(const std::string &filename = "");

        //---------------------------------------------------
        // CEC check
        fastLEC::ret_vals check_cec();
    };

}