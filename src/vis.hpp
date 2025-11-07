#pragma once

#include "XAG.hpp"
#include "CNF.hpp"
#include <memory>
#include <vector>
#include <string>

namespace fastLEC
{

class Visualizer
{
public:
    std::shared_ptr<fastLEC::XAG> xag;
    std::shared_ptr<fastLEC::CNF> cnf;

    Visualizer(std::shared_ptr<fastLEC::XAG> xag);
    ~Visualizer() = default;

    std::string gen_basename_str();
    std::string gen_basename_str(std::vector<int> unit_clauses);

    void call_external_program_for_runtime_generation();

    void visualize(std::vector<int> unit_clauses);
};

} // namespace fastLEC