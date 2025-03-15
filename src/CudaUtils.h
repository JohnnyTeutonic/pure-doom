#ifndef CUDA_UTILS_H
#define CUDA_UTILS_H

#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <iostream>

// CUDA compatibility macro
#ifdef __CUDACC__
#define CUDA_CALLABLE __host__ __device__
#else
#define CUDA_CALLABLE
#endif

namespace PureDoom {

// Error checking macro for CUDA calls
#define CUDA_CHECK(call) \
    { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            std::cerr << "CUDA error in " << __FILE__ << " at line " << __LINE__ << ": " \
                      << cudaGetErrorString(err) << std::endl; \
            exit(EXIT_FAILURE); \
        } \
    }

// Struct for storing CUDA device properties
struct CudaDeviceInfo {
    int deviceCount;
    cudaDeviceProp deviceProperties;
    bool cudaAvailable;
    
    CudaDeviceInfo() : deviceCount(0), cudaAvailable(false) {}
};

// Initialize CUDA and return device information
CudaDeviceInfo initializeCuda();

// Cleanup CUDA resources
void cleanupCuda();

// Get optimal thread block size for a specific CUDA kernel
dim3 getOptimalBlockSize(int width, int height);

// Debug function to print device information
void printCudaDeviceInfo(const CudaDeviceInfo& info);

} // namespace PureDoom

#endif // CUDA_UTILS_H 