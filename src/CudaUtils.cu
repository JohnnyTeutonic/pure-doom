#include "CudaUtils.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <iostream>

namespace PureDoom {

CudaDeviceInfo initializeCuda() {
    CudaDeviceInfo info;
    
    // Check for CUDA devices
    cudaError_t error = cudaGetDeviceCount(&info.deviceCount);
    if (error != cudaSuccess || info.deviceCount == 0) {
        info.cudaAvailable = false;
        std::cerr << "CUDA initialization error: " 
                  << (error != cudaSuccess ? cudaGetErrorString(error) : "No CUDA devices found")
                  << std::endl;
        return info;
    }
    
    // Get properties of the first CUDA device
    error = cudaGetDeviceProperties(&info.deviceProperties, 0);
    if (error != cudaSuccess) {
        info.cudaAvailable = false;
        std::cerr << "Failed to get CUDA device properties: " 
                  << cudaGetErrorString(error) << std::endl;
        return info;
    }
    
    // Set the current device
    error = cudaSetDevice(0);
    if (error != cudaSuccess) {
        info.cudaAvailable = false;
        std::cerr << "Failed to set CUDA device: " 
                  << cudaGetErrorString(error) << std::endl;
        return info;
    }
    
    // CUDA is available and initialized
    info.cudaAvailable = true;
    return info;
}

void cleanupCuda() {
    // Reset the CUDA device to clean up all resources
    cudaDeviceReset();
}

dim3 getOptimalBlockSize(int width, int height) {
    // Default block size for general-purpose kernels
    // 16x16 is a good starting point for 2D operations
    return dim3(16, 16);
}

void printCudaDeviceInfo(const CudaDeviceInfo& info) {
    if (!info.cudaAvailable) {
        std::cout << "No CUDA-capable devices found." << std::endl;
        return;
    }
    
    std::cout << "\n--- CUDA Device Information ---" << std::endl;
    std::cout << "Device name: " << info.deviceProperties.name << std::endl;
    std::cout << "Compute capability: " 
              << info.deviceProperties.major << "." 
              << info.deviceProperties.minor << std::endl;
    std::cout << "Global memory: " 
              << info.deviceProperties.totalGlobalMem / (1024 * 1024) << " MB" << std::endl;
    std::cout << "Multiprocessors: " << info.deviceProperties.multiProcessorCount << std::endl;
    std::cout << "Max threads per block: " << info.deviceProperties.maxThreadsPerBlock << std::endl;
    std::cout << "Max threads dimensions: (" 
              << info.deviceProperties.maxThreadsDim[0] << ", "
              << info.deviceProperties.maxThreadsDim[1] << ", "
              << info.deviceProperties.maxThreadsDim[2] << ")" << std::endl;
    std::cout << "Max grid dimensions: (" 
              << info.deviceProperties.maxGridSize[0] << ", "
              << info.deviceProperties.maxGridSize[1] << ", "
              << info.deviceProperties.maxGridSize[2] << ")" << std::endl;
    std::cout << "Warp size: " << info.deviceProperties.warpSize << std::endl;
    std::cout << "Shared memory per block: " 
              << info.deviceProperties.sharedMemPerBlock / 1024 << " KB" << std::endl;
    std::cout << "-------------------------------\n" << std::endl;
}

} // namespace PureDoom 