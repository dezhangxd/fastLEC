#pragma once

#include "XAG.hpp"
#include "basic.hpp"

namespace fastLEC
{

    // Instruction Simulator
    class ISimulator
    {
    public:
        ISimulator() = default;
        ~ISimulator() = default;
    };


    // the origin ES method in hybrid-CEC
    class Simulator
    {
        fastLEC::XAG &xag;
        std::unique_ptr<fastLEC::ISimulator> is = nullptr;

    public:
        Simulator(fastLEC::XAG &xag): xag(xag) {}
        ~Simulator() = default;

        bool construct_isimu();

        fastLEC::ret_vals run_ies();
        fastLEC::ret_vals run_es();
    
    private:
        XAG *xag;
    };

}