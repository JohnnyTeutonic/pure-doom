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

// Get information about available CUDA devices
CudaDeviceInfo getCudaDeviceInfo() {
    CudaDeviceInfo info;
    
    // Check if CUDA is available
    cudaError_t err = cudaGetDeviceCount(&info.deviceCount);
    if (err != cudaSuccess || info.deviceCount == 0) {
        info.available = false;
        info.cudaAvailable = false;
        std::cout << "CUDA is not available: " << cudaGetErrorString(err) << std::endl;
        return info;
    }
    
    info.cudaAvailable = true;
    info.available = true;
    
    // Get properties of the first device
    cudaDeviceProp deviceProperties;
    cudaGetDeviceProperties(&deviceProperties, 0);
    
    // Fill in device information
    info.name = deviceProperties.name;
    info.totalMemory = deviceProperties.totalGlobalMem / (1024 * 1024); // Convert to MB
    info.computeCapability = deviceProperties.major * 10 + deviceProperties.minor;
    info.multiProcessorCount = deviceProperties.multiProcessorCount;
    
    info.deviceProperties = deviceProperties;
    
    return info;
}

// CUDA helper functions for BSP operations
__device__ float dotProduct(const CudaVec2& v1, const CudaVec2& v2) {
    return v1.x * v2.x + v1.y * v2.y;
}

__device__ float crossProduct(const CudaVec2& v1, const CudaVec2& v2) {
    return v1.x * v2.y - v1.y * v2.x;
}

__device__ float length(const CudaVec2& v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

__device__ CudaVec2 normalize(const CudaVec2& v) {
    float len = length(v);
    if (len < 0.0001f) return CudaVec2(0.0f, 0.0f);
    return CudaVec2(v.x / len, v.y / len);
}

__device__ CudaVec2 subtract(const CudaVec2& v1, const CudaVec2& v2) {
    return CudaVec2(v1.x - v2.x, v1.y - v2.y);
}

__device__ CudaVec2 add(const CudaVec2& v1, const CudaVec2& v2) {
    return CudaVec2(v1.x + v2.x, v1.y + v2.y);
}

__device__ CudaVec2 multiply(const CudaVec2& v, float scalar) {
    return CudaVec2(v.x * scalar, v.y * scalar);
}

// Ray-Line intersection test for CUDA
__device__ bool rayLineIntersection(
    const CudaVec2& rayOrigin, 
    const CudaVec2& rayDir,
    const CudaLine& line,
    float& outDistance,
    float& outU
) {
    CudaVec2 v1 = subtract(rayOrigin, line.start);
    CudaVec2 v2 = subtract(line.end, line.start);
    CudaVec2 v3 = CudaVec2(-rayDir.y, rayDir.x);
    
    float dot = dotProduct(v2, v3);
    if (fabsf(dot) < 0.0001f) {
        return false; // Parallel or coincident
    }
    
    float t1 = crossProduct(v2, v1) / dot;
    float t2 = dotProduct(v1, v3) / dot;
    
    if (t1 >= 0.0f && t2 >= 0.0f && t2 <= 1.0f) {
        outDistance = t1;
        outU = t2;
        return true;
    }
    
    return false;
}

// Cast a ray through the serialized BSP tree
__device__ CudaWallCollision castRayBSP(
    const CudaBSPTree& bsp,
    const CudaVec2& rayOrigin,
    const CudaVec2& rayDir,
    float maxDistance
) {
    // Initialize collision data
    CudaWallCollision collision;
    collision.collision = false;
    collision.distance = maxDistance;
    
    // Stack-based BSP traversal (non-recursive)
    const int MAX_DEPTH = 32; // Maximum tree depth
    int nodeStack[MAX_DEPTH];
    int stackPos = 0;
    
    // Start at the root node
    nodeStack[stackPos++] = bsp.rootNodeIndex;
    
    while (stackPos > 0) {
        // Pop node from stack
        int nodeIndex = nodeStack[--stackPos];
        if (nodeIndex < 0 || nodeIndex >= bsp.nodeCount) continue;
        
        const CudaBSPNode& node = bsp.nodes[nodeIndex];
        
        if (node.isLeaf) {
            // Check all walls in this leaf node
            for (int i = 0; i < node.wallCount; ++i) {
                int wallIndex = node.wallStartIndex + i;
                if (wallIndex < 0 || wallIndex >= bsp.wallCount) continue;
                
                const CudaWall& wall = bsp.walls[wallIndex];
                float distance, u;
                
                if (rayLineIntersection(rayOrigin, rayDir, wall.segment, distance, u)) {
                    if (distance > 0.0001f && distance < collision.distance) {
                        collision.collision = true;
                        collision.distance = distance;
                        collision.wallHeight = 1.0f; // Standard wall height
                        collision.textureId = wall.textureId;
                        collision.texCoordU = u;
                        
                        // Get sector information
                        int sectorId = wall.sectorFront;
                        if (sectorId >= 0 && sectorId < bsp.sectorCount) {
                            collision.floorHeight = bsp.sectors[sectorId].floorHeight;
                            collision.ceilingHeight = bsp.sectors[sectorId].ceilingHeight;
                            collision.lightLevel = bsp.sectors[sectorId].lightLevel;
                        } else {
                            collision.floorHeight = 0.0f;
                            collision.ceilingHeight = 1.0f;
                            collision.lightLevel = 255;
                        }
                        
                        collision.isPortal = (wall.sectorBack != -1);
                    }
                }
            }
        } else {
            // Determine which side of the partitioner the ray origin is on
            CudaVec2 toStart = subtract(rayOrigin, node.partitioner.start);
            CudaVec2 partitionerDir = subtract(node.partitioner.end, node.partitioner.start);
            float side = crossProduct(toStart, partitionerDir);
            
            int nearNodeIndex, farNodeIndex;
            if (side >= 0.0f) {
                // Origin is in front of the partitioner
                nearNodeIndex = node.frontNodeIndex;
                farNodeIndex = node.backNodeIndex;
            } else {
                // Origin is behind the partitioner
                nearNodeIndex = node.backNodeIndex;
                farNodeIndex = node.frontNodeIndex;
            }
            
            // Always process the near side first
            if (nearNodeIndex >= 0) {
                nodeStack[stackPos++] = nearNodeIndex;
            }
            
            // Check if the ray intersects the partitioner
            float distance, u;
            if (rayLineIntersection(rayOrigin, rayDir, node.partitioner, distance, u)) {
                if (distance > 0.0001f && distance < collision.distance) {
                    // Ray intersects the partitioner, explore the far side as well
                    if (farNodeIndex >= 0) {
                        nodeStack[stackPos++] = farNodeIndex;
                    }
                }
            }
        }
    }
    
    return collision;
}

} // namespace PureDoom 