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
        Simulator() = delete;
        Simulator(fastLEC::XAG &xag) : xag(xag) {}
        ~Simulator() = default;

        bool construct_isimu();

        unsigned bv_bits, para_bits, batch_bits;
        void cal_es_bits(unsigned threads_for_es = 1);

        fastLEC::ret_vals run_es();
        fastLEC::ret_vals run_ies();
    };

} // namespace fastLEC