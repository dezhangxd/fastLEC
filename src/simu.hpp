#pragma once

#include "XAG.hpp"
#include "basic.hpp"

namespace fastLEC
{

    // Instruction Simulator
    typedef uint64_t bvec_t; // other bit-vector
    class ISimulator
    {

        bvec_t festivals[6] = {
            0xAAAAAAAAAAAAAAAA,
            0xCCCCCCCCCCCCCCCC,
            0xF0F0F0F0F0F0F0F0,
            0xFF00FF00FF00FF00,
            0xFFFF0000FFFF0000,
            0xFFFFFFFF00000000};

        void bvec_set(bvec_t *vec)
        {
            *vec = ~0ull;
        }

        void bvec_reset(bvec_t *vec)
        {
            *vec = 0ull;
        }

        // the operation type
        typedef enum op_type
        {
            OP_AND,  // OP_AND addr1 addr2 addr3 : mem[addr1] <- mem[addr2] & mem[addr3]
            OP_XOR,  // OP_XOR addr1 addr2 addr3 : mem[addr1] <- mem[addr2] ^ mem[addr3]
            OP_NOT,  // OP_NOT addr1 addr2 : mem[addr1] <- ~mem[addr2]
            OP_SAVE, // OP_SAVE addr1 cp_mem_addr : mem[addr1] <- copy_mems[cp_mem_addr]
        } op_type;

        // the longest circuit width should not longer than 2^16.
        typedef struct operation
        {
            op_type type : 2;
            bvec_t addr1 : 16;
            bvec_t addr2 : 16; // or const
            bvec_t addr3 : 16;
        } operation;

        typedef struct glob_ES
        {
            uint32_t PI_num, PO_lit;
            uint32_t mem_sz;

            uint32_t n_ops;
            operation *ops;
        } glob_ES;

        glob_ES glob_es;
        const uint16_t const0_addr = 0;
        const uint16_t const1_addr = 1;
        const unsigned NOT_ALLOC = -1;
        const unsigned BVEC_BIT_WIDTH = 6;
        const unsigned BVEC_SIZE = 64;

    public:
        ISimulator() = default;
        ~ISimulator()
        {
            if (glob_es.ops != nullptr)
                free(glob_es.ops);
        }

        void init_glob_ES(fastLEC::XAG &xag);

        void prt_bvec(bvec_t *vec);
        void prt_op(operation *op);

        int run_ies_round(uint64_t r);
        int run_ies();
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

        unsigned bv_bits, para_bits, batch_bits;
        void cal_es_bits(unsigned threads_for_es = 1);

        fastLEC::ret_vals run_es();
        fastLEC::ret_vals run_ies();
        fastLEC::ret_vals run_ges();
    };

} // namespace fastLEC