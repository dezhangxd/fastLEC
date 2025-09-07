#pragma once

#include <string>
#include <memory>
#include "AIG.hpp"   // AIGER
#include "XAG.hpp"   // XAG
#include "CNF.hpp"   // CNF
#include "basic.hpp" // basic

namespace fastLEC
{

    class FormatManager
    {
    private:
        std::shared_ptr<fastLEC::AIG> aig;
        std::shared_ptr<fastLEC::XAG> xag;
        std::shared_ptr<fastLEC::CNF> cnf;
        
    public:
        FormatManager() = default;
        ~FormatManager() = default;
        
        // Setter methods - receive shared_ptr objects
        void set_aig(std::shared_ptr<fastLEC::AIG> aig_ptr);
        void set_xag(std::shared_ptr<fastLEC::XAG> xag_ptr);
        void set_cnf(std::shared_ptr<fastLEC::CNF> cnf_ptr);
        
        // Format conversion methods
        bool aig_to_xag();
        bool aig_to_cnf();
        bool xag_to_cnf();
        bool xag_to_aig();  // if reverse conversion is needed
        bool cnf_to_xag();  // if reverse conversion is needed
        
        // Access methods
        const fastLEC::AIG* get_aig() const { return aig.get(); }
        const fastLEC::XAG* get_xag() const { return xag.get(); }
        const fastLEC::CNF* get_cnf() const { return cnf.get(); }
        
        fastLEC::AIG* get_aig() { return aig.get(); }
        fastLEC::XAG* get_xag() { return xag.get(); }
        fastLEC::CNF* get_cnf() { return cnf.get(); }
        
        // Status checking
        bool has_aig() const { return aig != nullptr; }
        bool has_xag() const { return xag != nullptr; }
        bool has_cnf() const { return cnf != nullptr; }
        
        // Resource management
        void clear_aig();
        void clear_xag();
        void clear_cnf();
        void clear_all();
        
        // Utility methods
        void print_status() const;
        std::string get_format_info() const;
        
        // Convenience methods for common workflows
        bool load_aig_and_convert_to_xag(const std::string &filename);
        bool load_aig_and_convert_to_cnf(const std::string &filename);
        
        // Get shared_ptr methods (for sharing ownership)
        std::shared_ptr<fastLEC::AIG> get_aig_shared() { return aig; }
        std::shared_ptr<fastLEC::XAG> get_xag_shared() { return xag; }
        std::shared_ptr<fastLEC::CNF> get_cnf_shared() { return cnf; }
    };

    class Prover
    {
    private:
        std::shared_ptr<fastLEC::AIG> aig;

    public:
        Prover() = default;
        ~Prover() = default;

        //---------------------------------------------------
        // read aiger filename from Param if filename = ""
        bool read_aiger(const std::string &filename = "");

        //---------------------------------------------------
        fastLEC::ret_vals fast_aig_check(std::shared_ptr<fastLEC::AIG> aig);
        fastLEC::ret_vals seq_SAT_kissat(std::shared_ptr<fastLEC::CNF> cnf);
        fastLEC::ret_vals seq_BDD_cudd(std::shared_ptr<fastLEC::XAG> xag);
        fastLEC::ret_vals seq_ES(std::shared_ptr<fastLEC::XAG> xag);
        fastLEC::ret_vals para_ES(std::shared_ptr<fastLEC::XAG> xag, int n_thread = 1);
        fastLEC::ret_vals gpu_ES(std::shared_ptr<fastLEC::XAG> xag);

        //---------------------------------------------------
        // CEC check
        fastLEC::ret_vals check_cec();
    };

}