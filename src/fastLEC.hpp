#pragma once

#include <string>
#include <memory>
#include "AIG.hpp" // AIGER
#include "XAG.hpp" // XAG
#include "CNF.hpp" // CNF
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

    // 禁用拷贝构造和赋值
    Prover(const Prover&) = delete;
    Prover& operator=(const Prover&) = delete;

    // 允许移动构造和移动赋值
    Prover(Prover&&) = default;
    Prover& operator=(Prover&&) = default;

    bool read_aiger(const std::string &filename = "");

    bool construct_XAG_from_AIG();

    bool construct_CNF_from_XAG();
    
    fastLEC::ret_vals check_cec();

    // Getter方法，返回const引用以避免不必要的拷贝
    const fastLEC::AIG& get_aig() const { return *aig; }
    const fastLEC::XAG& get_xag() const { return *xag; }
    const fastLEC::CNF& get_cnf() const { return *cnf; }

    // 非const版本，用于修改
    fastLEC::AIG& get_aig() { return *aig; }
    fastLEC::XAG& get_xag() { return *xag; }
    fastLEC::CNF& get_cnf() { return *cnf; }

    // 检查对象是否已构造
    bool has_aig() const { return aig != nullptr; }
    bool has_xag() const { return xag != nullptr; }
    bool has_cnf() const { return cnf != nullptr; }
};

}