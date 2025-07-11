#pragma once

#include <vector>
#include <memory>
#include <string>

extern "C"
{
#include "../deps/aiger/aiger.h"
}

namespace fastLEC
{

    // AIGER
    class AIG
    {
        std::shared_ptr<aiger> aig = nullptr;

    public:
        AIG() = default;
        ~AIG() = default;

        void set(std::shared_ptr<aiger> aig);
        std::shared_ptr<aiger> get();
        std::shared_ptr<aiger> create();

        bool construct(const std::string &filename);

        void rewrite();

        bool log(const std::string &filename);
        
    };

}