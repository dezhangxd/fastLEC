#include "simu.hpp"

// CUDA Hello World 示例
#include <cuda_runtime.h>
#include <cstdio>
#include <cmath>

// Batch processing kernel: handle multiple rounds
__global__ void batch_simulation_kernel(
    operation *ops,               // operation array
    unsigned n_ops,               // number of operations
    unsigned mem_sz,              // memory size
    unsigned PO_lit,              // output position
    unsigned long long start_r,   // starting round
    unsigned bv_bits,             // bit vector bits
    unsigned PI_num,              // number of inputs
    bvec_t *festivals,            // fixed input patterns
    int *results,                 // result array
    unsigned long long batch_size // batch size
)
{
    // Calculate the round processed by current thread
    unsigned long long thread_id = blockIdx.x * blockDim.x + threadIdx.x;
    if (thread_id >= batch_size)
        return;

    unsigned long long r = start_r + thread_id;

    // Allocate local memory - each thread gets its own portion of shared memory
    extern __shared__ bvec_t shared_mem[];
    bvec_t *local_mems = &shared_mem[threadIdx.x * mem_sz];

    // Initialize all memory to zero first
    for (unsigned i = 0; i < mem_sz; i++)
    {
        local_mems[i] = 0ull;
    }

    // Initialize constants
    local_mems[0] = 0ull;  // const0
    local_mems[1] = ~0ull; // const1

    // Set fixed input patterns
    for (unsigned i = 0; i < bv_bits; i++)
    {
        local_mems[i + 2] = festivals[i];
    }

    // Set other inputs based on round
    for (unsigned i = bv_bits; i < PI_num; i++)
    {
        unsigned long long k = (r >> (i - bv_bits)) & 1ull;
        if (k == 1)
            local_mems[i] = ~0ull;
        else
            local_mems[i] = 0ull;
    }

    // Execute simulation operations
    for (unsigned i = 0; i < n_ops; i++)
    {
        operation op = ops[i];

        if (op.type == OP_AND)
            local_mems[op.addr1] = local_mems[op.addr2] & local_mems[op.addr3];
        else if (op.type == OP_XOR)
            local_mems[op.addr1] = local_mems[op.addr2] ^ local_mems[op.addr3];
        else if (op.type == OP_NOT)
            local_mems[op.addr1] = ~local_mems[op.addr2];
    }

    // Check result
    if (local_mems[PO_lit] != 0u)
    {
        results[thread_id] = 10; // SAT
    }
    else
    {
        results[thread_id] = 20; // UNS
    }
}

// Batch processing kernel using 32-bit bit vectors (bvec_ts)
__global__ void batch_simulation_kernel_small(
    operation *ops,               // operation array
    unsigned n_ops,               // number of operations
    unsigned mem_sz,              // memory size
    unsigned PO_lit,              // output position
    unsigned long long start_r,   // starting round
    unsigned bv_bits,             // bit vector bits
    unsigned PI_num,              // number of inputs
    bvec_ts *festivals,           // fixed input patterns (32-bit)
    int *results,                 // result array
    unsigned long long batch_size // batch size
)
{
    // Calculate the round processed by current thread
    unsigned long long thread_id = blockIdx.x * blockDim.x + threadIdx.x;
    if (thread_id >= batch_size)
        return;

    unsigned long long r = start_r + thread_id;

    // Allocate local memory - each thread gets its own portion of shared memory
    extern __shared__ bvec_ts shared_mem_small[];
    bvec_ts *local_mems = &shared_mem_small[threadIdx.x * mem_sz];

    // Initialize all memory to zero first
    for (unsigned i = 0; i < mem_sz; i++)
    {
        local_mems[i] = 0u;
    }

    // Initialize constants
    local_mems[0] = 0u;   // const0
    local_mems[1] = ~0u;  // const1

    // Set fixed input patterns
    for (unsigned i = 0; i < bv_bits; i++)
    {
        local_mems[i + 2] = festivals[i];
    }

    // Set other inputs based on round
    for (unsigned i = bv_bits; i < PI_num; i++)
    {
        unsigned long long k = (r >> (i - bv_bits)) & 1ull;
        if (k == 1)
            local_mems[i] = ~0u;
        else
            local_mems[i] = 0u;
    }

    // Execute simulation operations
    for (unsigned i = 0; i < n_ops; i++)
    {
        operation op = ops[i];

        if (op.type == OP_AND)
            local_mems[op.addr1] = local_mems[op.addr2] & local_mems[op.addr3];
        else if (op.type == OP_XOR)
            local_mems[op.addr1] = local_mems[op.addr2] ^ local_mems[op.addr3];
        else if (op.type == OP_NOT)
            local_mems[op.addr1] = ~local_mems[op.addr2];
    }

    // Check result
    if (local_mems[PO_lit] != 0u)
    {
        results[thread_id] = 10; // SAT
    }
    else
    {
        results[thread_id] = 20; // UNS
    }
}

// GPU configuration structure
struct GPUConfig
{
    unsigned threadsPerBlock;
    unsigned maxBlocksPerGrid;
    unsigned long long maxConcurrentThreads;
    unsigned long long maxBatchSize;
    unsigned sharedMemSize;
    bool useSmallBitVector;  // true for bvec_ts (32-bit), false for bvec_t (64-bit)
    bool isValid;

    GPUConfig() : threadsPerBlock(0), maxBlocksPerGrid(0), maxConcurrentThreads(0),
                  maxBatchSize(0), sharedMemSize(0), useSmallBitVector(false), isValid(false) {}
};

// Function to calculate optimal thread block size
unsigned calculate_optimal_threads_per_block(const cudaDeviceProp &prop, unsigned shared_mem_per_thread)
{
    unsigned optimal_threads = 256; // default

    // Base selection on compute capability and SM count
    if (prop.major >= 8)
    { // Ampere, Ada, Hopper
        optimal_threads = 512;
        if (prop.multiProcessorCount >= 80)
        { // High-end GPUs
            optimal_threads = 1024;
        }
    }
    else if (prop.major >= 7)
    { // Volta, Turing
        optimal_threads = 512;
        if (prop.multiProcessorCount >= 60)
        { // High-end GPUs
            optimal_threads = 1024;
        }
    }
    else if (prop.major >= 6)
    { // Pascal
        optimal_threads = 256;
        if (prop.multiProcessorCount >= 20)
        { // High-end GPUs
            optimal_threads = 512;
        }
    }
    else
    { // Maxwell and older
        optimal_threads = 256;
    }

    // Consider shared memory constraints (shared_mem_per_thread is per thread)
    unsigned max_threads_by_shared_mem = prop.sharedMemPerBlock / shared_mem_per_thread;
    if (max_threads_by_shared_mem < optimal_threads)
    {
        optimal_threads = max_threads_by_shared_mem;
    }

    // Consider register constraints (approximate)
    unsigned max_threads_by_registers = prop.regsPerBlock / 32; // Assume 32 registers per thread
    if (max_threads_by_registers < optimal_threads)
    {
        optimal_threads = max_threads_by_registers;
    }

    // Ensure we don't exceed the maximum threads per block
    if (optimal_threads > prop.maxThreadsPerBlock)
    {
        optimal_threads = prop.maxThreadsPerBlock;
    }

    // Ensure threadsPerBlock is a multiple of warp size (32)
    optimal_threads = (optimal_threads / 32) * 32;

    // Ensure minimum of 32 threads (1 warp)
    if (optimal_threads < 32)
    {
        optimal_threads = 32;
    }

    return optimal_threads;
}

// Function to validate GPU configuration
bool validate_gpu_config(const GPUConfig &config, const cudaDeviceProp &prop)
{
    bool valid = true;

    // Check thread block size
    if (config.threadsPerBlock < 32 || config.threadsPerBlock > prop.maxThreadsPerBlock)
    {
        printf("c [gpu] WARNING: Threads per block (%u) is outside valid range [32, %u]\n",
               config.threadsPerBlock, prop.maxThreadsPerBlock);
        valid = false;
    }

    // Check if threads per block is warp-aligned
    if (config.threadsPerBlock % 32 != 0)
    {
        printf("c [gpu] WARNING: Threads per block (%u) is not warp-aligned\n", config.threadsPerBlock);
        valid = false;
    }

    // Check shared memory usage
    if (config.sharedMemSize > prop.sharedMemPerBlock)
    {
        printf("c [gpu] ERROR: Shared memory usage (%u) exceeds limit (%lu)\n",
               config.sharedMemSize, prop.sharedMemPerBlock);
        valid = false;
    }

    // Check batch size efficiency
    if (config.maxBatchSize < 1000)
    {
        printf("c [gpu] WARNING: Batch size (%llu) is very small, may be inefficient\n", config.maxBatchSize);
    }

    return valid;
}

GPUConfig configure_gpu_parameters(unsigned mem_sz)
{
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0); // use the first GPU

    GPUConfig config;

    // First try with 64-bit bit vectors (bvec_t)
    unsigned sharedMemPerThread = mem_sz * sizeof(bvec_t);
    config.threadsPerBlock = calculate_optimal_threads_per_block(prop, sharedMemPerThread);
    config.sharedMemSize = config.threadsPerBlock * sharedMemPerThread;
    config.useSmallBitVector = false;

    if (config.sharedMemSize > prop.sharedMemPerBlock)
    {
        printf("c [gpu] WARNING: Required shared memory (%u bytes) exceeds limit (%lu bytes)\n",
               config.sharedMemSize, prop.sharedMemPerBlock);
        printf("c [gpu] - Each thread needs %u bytes, block has %u threads\n",
               sharedMemPerThread, config.threadsPerBlock);
        
        // Try with 32-bit bit vectors (bvec_ts)
        printf("c [gpu] Trying with 32-bit bit vectors (bvec_ts) instead...\n");
        sharedMemPerThread = mem_sz * sizeof(bvec_ts);
        config.threadsPerBlock = calculate_optimal_threads_per_block(prop, sharedMemPerThread);
        config.sharedMemSize = config.threadsPerBlock * sharedMemPerThread;
        config.useSmallBitVector = true;
        
        if (config.sharedMemSize > prop.sharedMemPerBlock)
        {
            printf("c [gpu] ERROR: Even with 32-bit bit vectors, shared memory (%u bytes) still exceeds limit (%lu bytes)\n",
                   config.sharedMemSize, prop.sharedMemPerBlock);
            printf("c [gpu] - Each thread needs %u bytes, block has %u threads\n",
                   sharedMemPerThread, config.threadsPerBlock);
            config.isValid = false;
            return config;
        }
        else
        {
            printf("c [gpu] SUCCESS: Using 32-bit bit vectors (bvec_ts) - shared memory: %u bytes\n",
                   config.sharedMemSize);
        }
    }
    else
    {
        printf("c [gpu] Using 64-bit bit vectors (bvec_t) - shared memory: %u bytes\n",
               config.sharedMemSize);
    }

    unsigned maxBlocksByCompute = prop.multiProcessorCount * 32; // 32 blocks per SM is a good starting point
    unsigned maxBlocksByGrid = (prop.maxGridSize[0] < 65536u) ? prop.maxGridSize[0] : 65536u;
    config.maxBlocksPerGrid = (maxBlocksByCompute < maxBlocksByGrid) ? maxBlocksByCompute : maxBlocksByGrid;

    config.maxConcurrentThreads = config.threadsPerBlock * config.maxBlocksPerGrid;

    // Calculate optimal batch size based on GPU architecture
    unsigned long long optimalBatchSize;
    if (prop.major >= 8)
    {                                  // Ampere, Ada, Hopper
        optimalBatchSize = 5000000ull; // 5M threads for modern GPUs
    }
    else if (prop.major >= 7)
    {                                  // Volta, Turing
        optimalBatchSize = 3000000ull; // 3M threads
    }
    else if (prop.major >= 6)
    {                                  // Pascal
        optimalBatchSize = 2000000ull; // 2M threads
    }
    else
    {                                  // Maxwell and older
        optimalBatchSize = 1000000ull; // 1M threads
    }

    // Ensure batch size is reasonable and doesn't exceed concurrent threads
    config.maxBatchSize = (config.maxConcurrentThreads < optimalBatchSize) ? config.maxConcurrentThreads : optimalBatchSize;

    // Ensure minimum batch size for efficiency
    if (config.maxBatchSize < 10000ull)
    {
        config.maxBatchSize = 10000ull; // At least 10K threads per batch
    }

    config.isValid = true;

    if (!validate_gpu_config(config, prop))
    {
        printf("c [gpu] GPU configuration double check failed\n");
    }

    return config;
}

bool shown = false;
void show_gpu_info(int dev)
{
    if (shown)
        return;
    shown = true;

    int s = 0, e = 0;
    if (dev != -1)
    {
        s = dev;
        e = dev + 1;
    }
    else
    {
        int deviceCount = 0;
        cudaGetDeviceCount(&deviceCount);
        printf("c [gpu] Detected %d CUDA device(s):\n", deviceCount);
        s = 0;
        e = deviceCount;
    }
    for (int dev = s; dev < e; ++dev)
    {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, dev);
        printf("c [gpu] Device %d: %s\n", dev, prop.name);
        printf("c [gpu] - Compute Cap.: %d.%d", prop.major, prop.minor);
        printf(", Glob. Mem.: %.2f GB", prop.totalGlobalMem / (1024.0 * 1024 * 1024));
        printf(", Multipro. Cnt: %d\n", prop.multiProcessorCount);
        printf("c [gpu] - Max Threads/Block: %d", prop.maxThreadsPerBlock);
        printf(", Max Grid Size: [2^%d, 2^%d, 2^%d]\n", (unsigned)std::log2(prop.maxGridSize[0]), (unsigned)std::log2(prop.maxGridSize[1]), (unsigned)std::log2(prop.maxGridSize[2]));
        printf("c [gpu] - Shared Mem. /Block: %.2f KB", prop.sharedMemPerBlock / 1024.0);
        printf(", Warp Size: %d\n", prop.warpSize);
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

int gpu_run(glob_ES *ges, int verbose)
{
    if (verbose > 1)
    {
        show_gpu_info();
        fflush(stdout);
    }

    // Todo:: If rounds are too few, use CPU version

    GPUConfig gpuConfig = configure_gpu_parameters(ges->mem_sz);

    // Check if GPU configuration is valid
    if (!gpuConfig.isValid)
    {
        printf("c ERROR: [gpu] GPU configuration failed, you should back to use CPU version\n");
        return 0; // fallback to CPU version
    }

    // bv_bits depends on the bit vector size being used
    unsigned bv_bits;
    if (gpuConfig.useSmallBitVector) {
        bv_bits = 5;  // 32-bit bit vectors can represent 32 bits, so 5 patterns
    } else {
        bv_bits = 6;  // 64-bit bit vectors can represent 64 bits, so 6 patterns
    }
    unsigned r_bits = (ges->PI_num > bv_bits) ? (ges->PI_num - bv_bits) : 0;
    unsigned long long r_max = 1ULL << r_bits;

    if (verbose > 1)
    {
        printf("c [gpu] PI_num = %u, PO_lit = %u, mem_sz = %u, n_ops = %u\n", ges->PI_num, ges->PO_lit, ges->mem_sz, ges->n_ops);
        printf("c [gpu] r_bits = %u, r_max = %llu\n", r_bits, r_max);
        fflush(stdout);
    }
    bvec_t festivals[6] = {
        0xAAAAAAAAAAAAAAAA,
        0xCCCCCCCCCCCCCCCC,
        0xF0F0F0F0F0F0F0F0,
        0xFF00FF00FF00FF00,
        0xFFFF0000FFFF0000,
        0xFFFFFFFF00000000};
    
    bvec_ts festivals_s[5] = {
        0xAAAAAAAA,
        0xCCCCCCCC,
        0xF0F0F0F0,
        0xFF00FF00,
        0xFFFF0000};

    // Allocate device memory
    operation *d_ops;
    int *d_results;
    void *d_festivals;  // Will be cast to appropriate type based on config

    cudaMalloc(&d_ops, ges->n_ops * sizeof(operation));
    cudaMalloc(&d_results, gpuConfig.maxBatchSize * sizeof(int));

    // Allocate and copy festivals based on bit vector size
    if (gpuConfig.useSmallBitVector) {
        cudaMalloc(&d_festivals, 5 * sizeof(bvec_ts));
        cudaMemcpy(d_festivals, festivals_s, 5 * sizeof(bvec_ts), cudaMemcpyHostToDevice);
    } else {
        cudaMalloc(&d_festivals, 6 * sizeof(bvec_t));
        cudaMemcpy(d_festivals, festivals, 6 * sizeof(bvec_t), cudaMemcpyHostToDevice);
    }

    cudaMemcpy(d_ops, ges->ops, ges->n_ops * sizeof(operation), cudaMemcpyHostToDevice);

    int glob_res = 0;

    // Process rounds in batches
    const unsigned long long batch_size = gpuConfig.maxBatchSize; // Use the calculated reasonable batch size
    const unsigned long long num_batches = (r_max + batch_size - 1) / batch_size;

    if (verbose > 1)
    {
        printf("c [gpu] Processing %llu rounds in %llu batches of %llu each\n",
               r_max, num_batches, batch_size);
        fflush(stdout);
    }

    for (unsigned long long batch = 0; batch < num_batches && glob_res != 10; batch++)
    {
        unsigned long long start_r = batch * batch_size;
        unsigned long long end_r = (batch + 1) * batch_size;
        if (end_r > r_max)
            end_r = r_max;

        unsigned long long current_batch_size = end_r - start_r;
        unsigned blocksPerGrid = (current_batch_size + gpuConfig.threadsPerBlock - 1) / gpuConfig.threadsPerBlock;

        // Launch kernel based on bit vector size
        if (gpuConfig.useSmallBitVector) {
            batch_simulation_kernel_small<<<blocksPerGrid, gpuConfig.threadsPerBlock, gpuConfig.sharedMemSize>>>(
                d_ops, ges->n_ops, ges->mem_sz, ges->PO_lit, start_r,
                bv_bits, ges->PI_num, (bvec_ts*)d_festivals, d_results, current_batch_size);
        } else {
            batch_simulation_kernel<<<blocksPerGrid, gpuConfig.threadsPerBlock, gpuConfig.sharedMemSize>>>(
                d_ops, ges->n_ops, ges->mem_sz, ges->PO_lit, start_r,
                bv_bits, ges->PI_num, (bvec_t*)d_festivals, d_results, current_batch_size);
        }

        // Check kernel execution error
        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess)
        {
            printf("c [gpu] CUDA kernel error: %s\n", cudaGetErrorString(err));
            break;
        }

        // Copy results back to host
        int *h_results = (int *)malloc(current_batch_size * sizeof(int));
        cudaMemcpy(h_results, d_results, current_batch_size * sizeof(int), cudaMemcpyDeviceToHost);

        // Check if there's a SAT result
        for (unsigned long long i = 0; i < current_batch_size; i++)
        {
            if (h_results[i] == 10)
            {
                glob_res = 10;
                break;
            }
        }

        free(h_results);

        if (verbose > 0 && batch % 100 == 0)
        {
            printf("c [gpu] Progress: %6.2f%% (%llu/%llu batches)\n",
                   (double)batch / num_batches * 100, batch, num_batches);
            fflush(stdout);
        }
    }

    // Free device memory
    cudaFree(d_ops);
    cudaFree(d_festivals);
    cudaFree(d_results);

    if (glob_res == 10)
        return 10;
    else
        return 20;
}