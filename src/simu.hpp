#pragma once

#include "XAG.hpp"
#include "basic.hpp"
#include "gES.h"

extern bvec_t festivals[6];
void bvec_set(bvec_t *vec);
void bvec_reset(bvec_t *vec);

namespace fastLEC
{
class ISimulator
{
public:
    glob_ES glob_es;

    const uint16_t const0_addr = 0;
    const uint16_t const1_addr = 1;
    const uint32_t NOT_ALLOC = UINT32_MAX; // Use UINT32_MAX instead of -1
    const unsigned BVEC_BIT_WIDTH = 6;
    const unsigned BVEC_SIZE = 64;

    ISimulator() = default;
    ~ISimulator()
    {
        if (glob_es.ops != nullptr)
            free(glob_es.ops);
    }

    void init_glob_ES(fastLEC::XAG &xag);
    void init_gpu_ES(fastLEC::XAG &xag, glob_ES **ges);

    void prt_bvec(bvec_t *vec);
    void prt_op(operation *op);

    fastLEC::ret_vals run_ies_round(uint64_t r);
    fastLEC::ret_vals run_ies();
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

    // sequential methods
    unsigned bv_bits, para_bits, batch_bits;
    void cal_es_bits(unsigned threads_for_es =
                         1);    // calculate the bv_bits, para_bits, batch_bits.
    fastLEC::ret_vals run_es(); // using normal ES method on original XAG
    fastLEC::ret_vals run_ies(); // ES with (instruction) compacted XAG

    // parallel methods
    unsigned cal_pes_threads(unsigned n_thread); // calculate the number of used
                                                 // threads for PES "round"
    fastLEC::ret_vals run_round_pes(unsigned n_t);
    fastLEC::ret_vals run_pbits_pes(unsigned n_t);

    // GPU parallel methods
    fastLEC::ret_vals run_ges(); // using GPU ES methods on compacted XAG
};

} // namespace fastLEC