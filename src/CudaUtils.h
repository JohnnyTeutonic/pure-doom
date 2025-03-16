#ifndef CUDA_UTILS_H
#define CUDA_UTILS_H

#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <iostream>
#include <stdio.h>
#include <string>
#include <stdexcept>

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
            std::string errorMsg = "CUDA error in " + std::string(__FILE__) + \
                                  " at line " + std::to_string(__LINE__) + ": " + \
                                  std::string(cudaGetErrorString(err)); \
            std::cerr << errorMsg << std::endl; \
            throw std::runtime_error(errorMsg); \
        } \
    }

// Struct for storing CUDA device properties
struct CudaDeviceInfo {
    int deviceCount;
    cudaDeviceProp deviceProperties;
    bool cudaAvailable;
    bool available;
    std::string name;
    int totalMemory;
    int computeCapability;
    int multiProcessorCount;
    
    CudaDeviceInfo() : deviceCount(0), cudaAvailable(false), available(false), totalMemory(0), computeCapability(0), multiProcessorCount(0) {}
};

// Initialize CUDA and return device information
CudaDeviceInfo initializeCuda();

// Cleanup CUDA resources
void cleanupCuda();

// Get optimal thread block size for a specific CUDA kernel
dim3 getOptimalBlockSize(int width, int height);

// Debug function to print device information
void printCudaDeviceInfo(const CudaDeviceInfo& info);

// Function to check for CUDA device and get its info
CudaDeviceInfo getCudaDeviceInfo();

// CUDA-friendly BSP tree structures for serialization
struct CudaVec2 {
    float x, y;
    
    __host__ __device__ CudaVec2() : x(0.0f), y(0.0f) {}
    __host__ __device__ CudaVec2(float x_, float y_) : x(x_), y(y_) {}
};

struct CudaLine {
    CudaVec2 start;
    CudaVec2 end;
    
    __host__ __device__ CudaLine() {}
    __host__ __device__ CudaLine(const CudaVec2& s, const CudaVec2& e) : start(s), end(e) {}
};

struct CudaWall {
    CudaLine segment;
    int sectorFront;
    int sectorBack;
    int textureId;
    float textureOffsetX;
    float textureOffsetY;
    int lightLevel;
    
    __host__ __device__ CudaWall() 
        : sectorFront(-1), sectorBack(-1), textureId(-1), 
          textureOffsetX(0.0f), textureOffsetY(0.0f), lightLevel(255) {}
};

struct CudaSector {
    int wallStartIndex;  // Index of the first wall in the global wall array
    int wallCount;       // Number of walls in this sector
    float floorHeight;
    float ceilingHeight;
    int floorTextureId;
    int ceilingTextureId;
    int lightLevel;
    
    __host__ __device__ CudaSector() 
        : wallStartIndex(-1), wallCount(0), floorHeight(0.0f), ceilingHeight(0.0f),
          floorTextureId(-1), ceilingTextureId(-1), lightLevel(255) {}
};

// Wall collision information for BSP ray casting
struct CudaWallCollision {
    bool collision;         // Whether a collision occurred
    float distance;         // Distance to collision
    int textureId;          // Texture ID of the hit wall
    float texCoordU;        // Texture horizontal coordinate (0-1)
    float wallHeight;       // Height of the wall
    float floorHeight;      // Floor height at collision point
    float ceilingHeight;    // Ceiling height at collision point
    bool isPortal;          // Whether the wall is a portal
    int lightLevel;         // Light level at the wall (0-255)
    int sectorFront;        // Front sector ID
    int sectorBack;         // Back sector ID (-1 if not a portal)
    
    __host__ __device__ CudaWallCollision() 
        : collision(false), distance(0.0f), textureId(0), texCoordU(0.0f),
          wallHeight(0.0f), floorHeight(0.0f), ceilingHeight(0.0f), 
          isPortal(false), lightLevel(255),
          sectorFront(-1), sectorBack(-1) {}
};

// Structure to hold sprite rendering data for CUDA
struct CudaSpriteData {
    float x, y;           // Position in world space
    float scale;          // Sprite scale factor
    int textureId;        // Texture ID for the sprite
    int type;             // Sprite type
    float distance;       // Distance to the sprite from player
    bool visible;         // Whether the sprite is visible
    
    __host__ __device__ CudaSpriteData() 
        : x(0.0f), y(0.0f), scale(1.0f), textureId(0), 
          type(0), distance(0.0f), visible(true) {}
};

// Data structure for BSP node serialization
struct CudaBSPNode {
    CudaLine partitioner;
    int frontNodeIndex;  // Index of the front child node in the nodes array
    int backNodeIndex;   // Index of the back child node in the nodes array
    int wallStartIndex;  // Index of the first wall in this node
    int wallCount;       // Number of walls in this node
    int sectorId;        // Sector ID for leaf nodes
    bool isLeaf;         // Whether this is a leaf node
    
    __host__ __device__ CudaBSPNode() 
        : frontNodeIndex(-1), backNodeIndex(-1), wallStartIndex(-1), 
          wallCount(0), sectorId(-1), isLeaf(false) {}
};

// Complete serialized BSP data for CUDA
struct CudaBSPTree {
    CudaBSPNode* nodes;        // Array of all BSP nodes
    int nodeCount;             // Number of nodes in the tree
    int rootNodeIndex;         // Index of the root node
    
    CudaWall* walls;           // Array of all walls
    int wallCount;             // Number of walls
    
    CudaSector* sectors;       // Array of all sectors
    int sectorCount;           // Number of sectors
    
    __host__ __device__ CudaBSPTree() 
        : nodes(nullptr), nodeCount(0), rootNodeIndex(0),
          walls(nullptr), wallCount(0),
          sectors(nullptr), sectorCount(0) {}
};

// Device function declarations for CUDA BSP operations
__device__ float dotProduct(const CudaVec2& v1, const CudaVec2& v2);
__device__ float crossProduct(const CudaVec2& v1, const CudaVec2& v2);
__device__ float length(const CudaVec2& v);
__device__ CudaVec2 normalize(const CudaVec2& v);
__device__ CudaVec2 subtract(const CudaVec2& v1, const CudaVec2& v2);
__device__ CudaVec2 add(const CudaVec2& v1, const CudaVec2& v2);
__device__ CudaVec2 multiply(const CudaVec2& v, float scalar);

// Ray-line intersection test for CUDA
__device__ bool rayLineIntersection(
    const CudaVec2& rayOrigin, 
    const CudaVec2& rayDir,
    const CudaLine& line,
    float& outDistance,
    float& outU
);

// Cast a ray through the serialized BSP tree - function declaration
__device__ CudaWallCollision castRayBSP(
    const CudaBSPTree& bsp,
    const CudaVec2& rayOrigin,
    const CudaVec2& rayDir,
    float maxDistance
);

} // namespace PureDoom

#endif // CUDA_UTILS_H 