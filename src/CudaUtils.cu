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

// Check if two vectors are approximately equal
__device__ bool vectorsEqual(const CudaVec2& v1, const CudaVec2& v2, float epsilon = 0.0001f) {
    return fabsf(v1.x - v2.x) < epsilon && fabsf(v1.y - v2.y) < epsilon;
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
    
    // Safety checks for null pointers or empty data
    if (bsp.nodes == nullptr || bsp.walls == nullptr || bsp.sectors == nullptr || 
        bsp.nodeCount <= 0 || bsp.wallCount <= 0 || bsp.sectorCount <= 0) {
        return collision;
    }
    
    // Validate root node index
    if (bsp.rootNodeIndex < 0 || bsp.rootNodeIndex >= bsp.nodeCount) {
        return collision;
    }
    
    // Stack-based BSP traversal (non-recursive)
    const int MAX_DEPTH = 64; // Maximum tree depth
    
    // Traversal stack - we store node index, origin, and remaining distance
    struct TraversalItem {
        int nodeIndex;
        CudaVec2 origin;
        float remainingDistance;
    };
    
    TraversalItem stack[MAX_DEPTH];
    int stackPos = 0;
    
    // Start with the root node
    stack[stackPos].nodeIndex = bsp.rootNodeIndex;
    stack[stackPos].origin = rayOrigin;
    stack[stackPos].remainingDistance = maxDistance;
    stackPos++;
    
    // Prevent infinite loops with a traversal counter
    int traversalCount = 0;
    const int MAX_TRAVERSALS = 2000; // Safety limit
    
    // Main BSP traversal loop
    while (stackPos > 0 && traversalCount < MAX_TRAVERSALS) {
        traversalCount++;
        
        // Pop item from the stack
        stackPos--;
        TraversalItem current = stack[stackPos];
        
        // Skip if we've exceeded the max distance
        if (current.remainingDistance <= 0.0f) {
            continue;
        }
        
        // Get the node
        if (current.nodeIndex < 0 || current.nodeIndex >= bsp.nodeCount) {
            continue; // Invalid node index
        }
        
        const CudaBSPNode& node = bsp.nodes[current.nodeIndex];
        
        // Check if this is a leaf node (sector)
        if (node.isLeaf) {
            // Skip if invalid sector ID
            if (node.sectorId < 0 || node.sectorId >= bsp.sectorCount) {
                continue;
            }
            
            const CudaSector& sector = bsp.sectors[node.sectorId];
            
            // Skip if invalid wall range
            if (sector.wallStartIndex < 0 || 
                sector.wallStartIndex + sector.wallCount > bsp.wallCount) {
                continue;
            }
            
            // Check all walls in this sector for intersections
            for (int i = 0; i < sector.wallCount; ++i) {
                int wallIndex = sector.wallStartIndex + i;
                if (wallIndex < 0 || wallIndex >= bsp.wallCount) {
                    continue; // Skip invalid wall index
                }
                
                const CudaWall& wall = bsp.walls[wallIndex];
                float distance, u;
                
                if (rayLineIntersection(current.origin, rayDir, wall.segment, distance, u)) {
                    // Calculate total distance from original ray origin
                    float totalDistance = vectorsEqual(current.origin, rayOrigin) ? 
                                distance : // We're at the original ray origin
                                (maxDistance - current.remainingDistance) + distance; // We're at a partition point
                    
                    // Ensure the intersection is valid and closer than current closest hit
                    if (distance > 0.0001f && totalDistance < collision.distance) {
                        // We found a valid wall hit
                        collision.collision = true;
                        collision.distance = totalDistance;
                        collision.textureId = wall.textureId;
                        collision.texCoordU = u;
                        
                        // Get sector information
                        collision.wallHeight = sector.ceilingHeight - sector.floorHeight;
                        collision.floorHeight = sector.floorHeight;
                        collision.ceilingHeight = sector.ceilingHeight;
                        collision.isPortal = (wall.sectorBack >= 0);
                        collision.lightLevel = wall.lightLevel;
                        
                        // Store additional information for portals
                        collision.sectorFront = wall.sectorFront;
                        collision.sectorBack = wall.sectorBack;
                        
                        // If this is a portal and we want to continue through it,
                        // we could push the back sector onto the stack here.
                        // For now, we just stop at the first wall hit.
                    }
                }
            }
            
            // Don't need to traverse further for this branch
            continue;
        }
        
        // For internal nodes (not leaf nodes)
        
        // Check which sides of the partitioner to traverse
        CudaVec2 partitionerDir = subtract(node.partitioner.end, node.partitioner.start);
        CudaVec2 normal = CudaVec2(-partitionerDir.y, partitionerDir.x); // Perpendicular to partitioner
        
        // Determine which side of the partitioner the ray origin is on
        CudaVec2 toPartStart = subtract(current.origin, node.partitioner.start);
        float side = dotProduct(normal, toPartStart);
        float dirSide = dotProduct(normal, rayDir);
        
        // Handle different cases based on ray position and direction
        if (side >= 0.0f) {
            // Origin is in front of the partitioner
            
            // Process front child first
            if (node.frontNodeIndex >= 0 && node.frontNodeIndex < bsp.nodeCount) {
                if (stackPos < MAX_DEPTH) {
                    stack[stackPos].nodeIndex = node.frontNodeIndex;
                    stack[stackPos].origin = current.origin;
                    stack[stackPos].remainingDistance = current.remainingDistance;
                    stackPos++;
                }
            }
            
            // Check if ray points to back side
            if (dirSide < 0.0f) {
                // Calculate intersection with partitioner
                float t = -side / dirSide;
                
                if (t > 0.0f && t < current.remainingDistance) {
                    // Calculate intersection point
                    CudaVec2 intersectionPoint = add(current.origin, multiply(rayDir, t));
                    
                    // Check if intersection is within partitioner segment
                    CudaVec2 lineDir = normalize(partitionerDir);
                    CudaVec2 toIntersection = subtract(intersectionPoint, node.partitioner.start);
                    float projection = dotProduct(toIntersection, lineDir);
                    float lineLength = length(partitionerDir);
                    
                    if (projection >= 0.0f && projection <= lineLength) {
                        // Process back child with updated origin and distance
                        if (node.backNodeIndex >= 0 && node.backNodeIndex < bsp.nodeCount && stackPos < MAX_DEPTH) {
                            stack[stackPos].nodeIndex = node.backNodeIndex;
                            stack[stackPos].origin = intersectionPoint;
                            stack[stackPos].remainingDistance = current.remainingDistance - t;
                            stackPos++;
                        }
                    }
                }
            }
        } else {
            // Origin is in back of the partitioner
            
            // Process back child first
            if (node.backNodeIndex >= 0 && node.backNodeIndex < bsp.nodeCount) {
                if (stackPos < MAX_DEPTH) {
                    stack[stackPos].nodeIndex = node.backNodeIndex;
                    stack[stackPos].origin = current.origin;
                    stack[stackPos].remainingDistance = current.remainingDistance;
                    stackPos++;
                }
            }
            
            // Check if ray points to front side
            if (dirSide > 0.0f) {
                // Calculate intersection with partitioner
                float t = -side / dirSide;
                
                if (t > 0.0f && t < current.remainingDistance) {
                    // Calculate intersection point
                    CudaVec2 intersectionPoint = add(current.origin, multiply(rayDir, t));
                    
                    // Check if intersection is within partitioner segment
                    CudaVec2 lineDir = normalize(partitionerDir);
                    CudaVec2 toIntersection = subtract(intersectionPoint, node.partitioner.start);
                    float projection = dotProduct(toIntersection, lineDir);
                    float lineLength = length(partitionerDir);
                    
                    if (projection >= 0.0f && projection <= lineLength) {
                        // Process front child with updated origin and distance
                        if (node.frontNodeIndex >= 0 && node.frontNodeIndex < bsp.nodeCount && stackPos < MAX_DEPTH) {
                            stack[stackPos].nodeIndex = node.frontNodeIndex;
                            stack[stackPos].origin = intersectionPoint;
                            stack[stackPos].remainingDistance = current.remainingDistance - t;
                            stackPos++;
                        }
                    }
                }
            }
        }
    }
    
    return collision;
}

} // namespace PureDoom 