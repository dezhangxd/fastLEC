#pragma once

#include <vector>
#include <memory>
#include <string>
#include <functional>

extern "C"
{
#include "../deps/aiger/aiger.h"
}

namespace fastLEC
{

    int aiger_var(const unsigned &aiger_lit);
    int aiger_pos_lit(const unsigned &aiger_var);
    int aiger_neg_lit(const unsigned &aiger_var);
    bool aiger_value(const unsigned &aiger_lit, const bool &aiger_var_val);
    bool aiger_has_same_var(const unsigned &aiger_lit0, const unsigned &aiger_lit1);

    // AIGER
    class AIG
    {
        std::unique_ptr<aiger, std::function<void(aiger *)>> aig = nullptr;

    public:
        AIG() = default;
        ~AIG() = default;

        void set(std::unique_ptr<aiger, std::function<void(aiger *)>> aig);
        std::unique_ptr<aiger, std::function<void(aiger *)>> move();
        auto create() -> std::unique_ptr<aiger, std::function<void(aiger *)>>;
        std::unique_ptr<aiger, std::function<void(aiger *)>> &get() { return aig; }
        const std::unique_ptr<aiger, std::function<void(aiger *)>> &get() const { return aig; }

        bool construct(const std::string &filename);

        void rewrite();

        bool log(const std::string &filename);

        // check and return the pointer if it is AND gate
        aiger_and *is_and(unsigned lit) const;

        // check if lit is an XOR gate, and return the two inputs of the XOR.
        bool is_xor(unsigned lit, unsigned *rhs0ptr, unsigned *rhs1ptr) const;
    };

}