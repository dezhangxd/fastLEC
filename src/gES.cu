#include "simu.hpp"

// CUDA Hello World 示例
#include <cuda_runtime.h>
#include <cstdio>

void show_gpu_info()
{
    // Output basic information about the current GPU(s)
    int deviceCount = 0;
    cudaGetDeviceCount(&deviceCount);
    printf("c [gpu] Detected %d CUDA device(s):\n", deviceCount);
    for (int dev = 0; dev < deviceCount; ++dev)
    {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, dev);
        printf("c [gpu] Device %d: %s\n", dev, prop.name);
        printf("c [gpu] - Compute Capability: %d.%d\n", prop.major, prop.minor);
        printf("c [gpu] - Global Memory: %.2f GB\n", prop.totalGlobalMem / (1024.0 * 1024 * 1024));
        printf("c [gpu] - Multiprocessor Count: %d\n", prop.multiProcessorCount);
        printf("c [gpu] - Max Threads per Block: %d\n", prop.maxThreadsPerBlock);
        printf("c [gpu] - Max Grid Size: [%d, %d, %d]\n", prop.maxGridSize[0], prop.maxGridSize[1], prop.maxGridSize[2]);
        printf("c [gpu] - Shared Memory per Block: %.2f KB\n", prop.sharedMemPerBlock / 1024.0);
        printf("c [gpu] - Warp Size: %d\n", prop.warpSize);
    }
}

glob_ES *gpu_init()
{
    glob_ES *xag = (glob_ES *)malloc(sizeof(glob_ES));
    xag->PI_num = 0;
    xag->PO_lit = 0;
    xag->mem_sz = 0;
    xag->n_ops = 0;
    xag->ops = nullptr;
    return xag;
}

void free_gpu(glob_ES *ges)
{
    if (ges->ops != nullptr)
        free(ges->ops);
    if (ges != nullptr)
        free(ges);
}

int gpu_run(glob_ES *ges)
{
    show_gpu_info();
    fflush(stdout);

    const unsigned bv_bits = 6;
    unsigned r_bits = (ges->PI_num > bv_bits) ? (ges->PI_num - bv_bits) : 0;
    unsigned long long r_max = 1 << r_bits;

    printf("c [gpu] PI_num = %u, PO_lit = %u, mem_sz = %u, n_ops = %u\n", ges->PI_num, ges->PO_lit, ges->mem_sz, ges->n_ops);
    printf("c [gpu] r_bits = %u, r_max = %llu\n", r_bits, r_max);

    fflush(stdout);

    int glob_res = 0;

    for (unsigned long long r = 0; r < r_max; r++)
    {
        if (glob_res == 10)
            break;
            
        unsigned long long i, k;
        operation *op;
        bvec_t *local_mems;
        local_mems = (bvec_t *)malloc(ges->mem_sz * sizeof(bvec_t));

        local_mems[0] = 0ull;
        local_mems[1] = ~0ull;
        i = 2;

        for (i = 0; i < bv_bits; i++)
            local_mems[i + 2] = festivals[i];

        for (i = bv_bits; i < ges->PI_num; i++)
        {
            k = (r >> (i - bv_bits)) & 1ull;
            if (k == 1)
                local_mems[i] = ~0ull;
            else
                local_mems[i] = 0ull;
        }

        // start to simulation
        for (i = 0; i < ges->n_ops; i++)
        {
            op = &ges->ops[i];

            if (op->type == OP_AND)
                local_mems[op->addr1] = local_mems[op->addr2] & local_mems[op->addr3];
            else if (op->type == OP_XOR)
                local_mems[op->addr1] = local_mems[op->addr2] ^ local_mems[op->addr3];
            else if (op->type == OP_NOT)
                local_mems[op->addr1] = ~local_mems[op->addr2];
        }

        if (local_mems[ges->PO_lit] != 0u)
            glob_res = 10;

        free(local_mems);
    }

    if (glob_res == 10)
        return 10;
    else
        return 20;
}