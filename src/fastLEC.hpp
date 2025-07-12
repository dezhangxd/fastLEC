#pragma once

#include <string>
#include <memory>
#include "AIG.hpp"   // AIGER
#include "XAG.hpp"   // XAG
#include "CNF.hpp"   // CNF
#include "basic.hpp" // basic

namespace fastLEC
{

    class Prover
    {
        std::unique_ptr<fastLEC::AIG> aig;
        std::unique_ptr<fastLEC::XAG> xag;
        std::unique_ptr<fastLEC::CNF> cnf;

    public:
        Prover() = default;
        ~Prover() = default;

        Prover(const Prover &) = delete;
        Prover &operator=(const Prover &) = delete;

        Prover(Prover &&) = default;
        Prover &operator=(Prover &&) = default;

        const fastLEC::AIG &get_aig() const { return *aig; }
        const fastLEC::XAG &get_xag() const { return *xag; }
        const fastLEC::CNF &get_cnf() const { return *cnf; }

        fastLEC::AIG &get_aig() { return *aig; }
        fastLEC::XAG &get_xag() { return *xag; }
        fastLEC::CNF &get_cnf() { return *cnf; }

        bool has_aig() const { return aig != nullptr; }
        bool has_xag() const { return xag != nullptr; }
        bool has_cnf() const { return cnf != nullptr; }

        //---------------------------------------------------
        // read aiger filename from Param if filename = ""
        bool read_aiger(const std::string &filename = "");

        //---------------------------------------------------
        // CEC check
        fastLEC::ret_vals check_cec();
    };

}