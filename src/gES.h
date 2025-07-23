#ifndef _gES_h_
#define _gES_h_

#include <cstdint>

#ifdef __cplusplus
extern "C"
{
#endif

    // Instruction Simulator
    typedef uint64_t bvec_t; // other bit-vector
    extern bvec_t festivals[6];
    extern void bvec_set(bvec_t *vec);
    extern void bvec_reset(bvec_t *vec);

    // the operation type
    typedef enum op_type
    {
        OP_AND, // OP_AND addr1 addr2 addr3 : mem[addr1] <- mem[addr2] & mem[addr3]
        OP_XOR, // OP_XOR addr1 addr2 addr3 : mem[addr1] <- mem[addr2] ^ mem[addr3]
        OP_NOT, // OP_NOT addr1 addr2 : mem[addr1] <- ~mem[addr2]
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

    glob_ES *gpu_init();

    int gpu_run(glob_ES *ges, int verbose = 0);

    void free_gpu(glob_ES *ges);

    void show_gpu_info(int dev = -1);

#ifdef __cplusplus
}
#endif

#endif