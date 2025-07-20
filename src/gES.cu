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
    return 0;
}