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
    
    // Use a larger epsilon to handle precision issues
    const float INTERSECTION_EPSILON = 0.00001f;
    
    // If lines are nearly parallel, handle as a special case
    if (fabsf(dot) < INTERSECTION_EPSILON) {
        // Check if ray and line are collinear
        float cross = crossProduct(v1, rayDir);
        if (fabsf(cross) < INTERSECTION_EPSILON) {
            // Collinear - check if they overlap
            float t1 = dotProduct(v1, rayDir) / dotProduct(rayDir, rayDir);
            float t2 = t1 + dotProduct(v2, rayDir) / dotProduct(rayDir, rayDir);
            
            // Sort t1 and t2
            if (t2 < t1) {
                float temp = t1;
                t1 = t2;
                t2 = temp;
            }
            
            // Check for overlap
            if (t1 <= 1.0f && t2 >= 0.0f) {
                // They overlap - use the closest positive intersection
                outDistance = fmaxf(0.0f, t1);
                outU = 0.5f; // Arbitrary texture coordinate for collinear case
                return true;
            }
        }
        return false; // Parallel but not collinear, or no overlap
    }
    
    float t1 = crossProduct(v2, v1) / dot;
    float t2 = dotProduct(v1, v3) / dot;
    
    // Add a small epsilon to the bounds check to handle edge cases
    if (t1 >= -INTERSECTION_EPSILON && t2 >= -INTERSECTION_EPSILON && t2 <= 1.0f + INTERSECTION_EPSILON) {
        // Make sure distance is positive (avoid numerical precision issues)
        if (t1 < INTERSECTION_EPSILON) {
            t1 = INTERSECTION_EPSILON;
        }
        
        outDistance = t1;
        
        // Clamp texCoordU to valid range to avoid texture sampling issues
        outU = t2;
        if (outU < 0.0f) outU = 0.0f;
        if (outU > 1.0f) outU = 1.0f;
        
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
    
    // Ensure ray direction is normalized for consistent distance calculations
    float rayLength = sqrtf(rayDir.x * rayDir.x + rayDir.y * rayDir.y);
    CudaVec2 normalizedRayDir;
    if (rayLength > 0.0001f) {
        normalizedRayDir = CudaVec2(rayDir.x / rayLength, rayDir.y / rayLength);
    } else {
        normalizedRayDir = CudaVec2(1.0f, 0.0f); // Default direction if ray is too short
    }
    
    // Linear scan for small test maps - faster and more reliable for debugging
    bool useLinearScan = (bsp.wallCount < 200); // Increased limit for better reliability
    
    if (useLinearScan) {
        // Simple linear scan of all walls for small maps
        float closestDistance = maxDistance;
        int closestWallIndex = -1;
        float closestU = 0.0f;
        
        // Use a multipass approach to improve precision
        // First pass: collect all potential intersections
        const int MAX_HITS = 16; // Maximum number of potential hits to track
        struct PotentialHit {
            int wallIndex;
            float distance;
            float texCoordU;
        };
        PotentialHit potentialHits[MAX_HITS];
        int numHits = 0;
        
        for (int i = 0; i < bsp.wallCount; i++) {
            const CudaWall& wall = bsp.walls[i];
            float distance, u;
            
            if (rayLineIntersection(rayOrigin, normalizedRayDir, wall.segment, distance, u)) {
                // Use a slightly smaller epsilon to prioritize closer hits but still avoid precision issues
                const float DISTANCE_EPSILON = 0.00001f;
                
                if (distance > DISTANCE_EPSILON && distance < maxDistance) {
                    // Store potential hit
                    if (numHits < MAX_HITS) {
                        potentialHits[numHits].wallIndex = i;
                        potentialHits[numHits].distance = distance;
                        potentialHits[numHits].texCoordU = u;
                        numHits++;
                    }
                    
                    // Also track the closest hit for early exit if we reach MAX_HITS
                    if (distance < closestDistance) {
                        closestDistance = distance;
                        closestWallIndex = i;
                        closestU = u;
                    }
                }
            }
        }
        
        // Second pass: sort hits by distance (simple bubble sort for small array)
        for (int i = 0; i < numHits - 1; i++) {
            for (int j = 0; j < numHits - i - 1; j++) {
                if (potentialHits[j].distance > potentialHits[j + 1].distance) {
                    // Swap
                    PotentialHit temp = potentialHits[j];
                    potentialHits[j] = potentialHits[j + 1];
                    potentialHits[j + 1] = temp;
                }
            }
        }
        
        // Third pass: check hits in order of distance
        closestWallIndex = -1; // Reset for final selection
        
        for (int i = 0; i < numHits; i++) {
            int wallIndex = potentialHits[i].wallIndex;
            const CudaWall& wall = bsp.walls[wallIndex];
            
            // Simple validation check - use the closest wall that has a valid sector
            if (wall.sectorFront >= 0 && wall.sectorFront < bsp.sectorCount) {
                closestWallIndex = wallIndex;
                closestDistance = potentialHits[i].distance;
                closestU = potentialHits[i].texCoordU;
                break; // Use the first valid wall
            }
        }
        
        // If we found a valid wall intersection
        if (closestWallIndex >= 0) {
            const CudaWall& wall = bsp.walls[closestWallIndex];
            int sectorIndex = wall.sectorFront;
            
            // Validate sector index
            if (sectorIndex >= 0 && sectorIndex < bsp.sectorCount) {
                const CudaSector& sector = bsp.sectors[sectorIndex];
                
                collision.collision = true;
                collision.distance = closestDistance;
                collision.textureId = wall.textureId;
                collision.texCoordU = closestU;
                
                // Get sector information
                collision.wallHeight = sector.ceilingHeight - sector.floorHeight;
                collision.floorHeight = sector.floorHeight;
                collision.ceilingHeight = sector.ceilingHeight;
                collision.isPortal = (wall.sectorBack >= 0);
                collision.lightLevel = wall.lightLevel;
                
                // Store wall index and sector ID for debugging
                collision.wallIndex = closestWallIndex;
                collision.sectorId = sectorIndex;
                
                // Store the wall normal for sliding calculations
                CudaVec2 wallDir = subtract(wall.segment.end, wall.segment.start);
                float wallLength = sqrtf(wallDir.x * wallDir.x + wallDir.y * wallDir.y);
                
                if (wallLength > 0.0001f) {
                    wallDir.x /= wallLength;
                    wallDir.y /= wallLength;
                    
                    // Normal is perpendicular to wall direction
                    collision.normal.x = -wallDir.y;
                    collision.normal.y = wallDir.x;
                    
                    // Make sure normal points away from wall
                    float dotProduct = collision.normal.x * normalizedRayDir.x + 
                                       collision.normal.y * normalizedRayDir.y;
                    
                    if (dotProduct > 0) {
                        // Flip normal if it's pointing in same direction as ray
                        collision.normal.x = -collision.normal.x;
                        collision.normal.y = -collision.normal.y;
                    }
                } else {
                    // Default normal if wall has zero length
                    collision.normal.x = -normalizedRayDir.x;
                    collision.normal.y = -normalizedRayDir.y;
                }
            }
        }
        
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
                
                if (rayLineIntersection(current.origin, normalizedRayDir, wall.segment, distance, u)) {
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
        
        // Normalize the normal vector
        float normalLength = sqrtf(normal.x * normal.x + normal.y * normal.y);
        if (normalLength > 0.0001f) {
            normal.x /= normalLength;
            normal.y /= normalLength;
        }
        
        // Determine which side of the partitioner the ray origin is on
        CudaVec2 toPartStart = subtract(current.origin, node.partitioner.start);
        float side = dotProduct(normal, toPartStart);
        float dirSide = dotProduct(normal, normalizedRayDir);
        
        // Use a small epsilon to avoid precision issues
        const float SIDE_EPSILON = 0.0001f;
        
        // Handle different cases based on ray position and direction
        if (side >= -SIDE_EPSILON) {
            // Origin is in front of or on the partitioner
            
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
                    CudaVec2 intersectionPoint = add(current.origin, multiply(normalizedRayDir, t));
                    
                    // Check if intersection is within partitioner segment
                    CudaVec2 lineDir = normalize(partitionerDir);
                    CudaVec2 toIntersection = subtract(intersectionPoint, node.partitioner.start);
                    float projection = dotProduct(toIntersection, lineDir);
                    float lineLength = length(partitionerDir);
                    
                    if (projection >= -SIDE_EPSILON && projection <= lineLength + SIDE_EPSILON) {
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
                    CudaVec2 intersectionPoint = add(current.origin, multiply(normalizedRayDir, t));
                    
                    // Check if intersection is within partitioner segment
                    CudaVec2 lineDir = normalize(partitionerDir);
                    CudaVec2 toIntersection = subtract(intersectionPoint, node.partitioner.start);
                    float projection = dotProduct(toIntersection, lineDir);
                    float lineLength = length(partitionerDir);
                    
                    if (projection >= -SIDE_EPSILON && projection <= lineLength + SIDE_EPSILON) {
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
    
    // Return the closest wall collision found during traversal
    return collision;
}

} // namespace PureDoom 