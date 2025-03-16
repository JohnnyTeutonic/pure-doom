#include "CudaUtils.h"
#include <vector>
#include <iostream>

namespace PureDoom {

// Creates a simple test map directly in CUDA-friendly format
// This bypasses the regular map system for debugging
CudaBSPTree createSimpleTestMap() {
    std::cout << "Creating simple test map for CUDA debugging..." << std::endl;
    
    // Initialize empty containers for our map data
    std::vector<CudaSector> sectors;
    std::vector<CudaWall> walls;
    std::vector<CudaBSPNode> nodes;
    
    // ======== DEFINE SECTORS ========
    
    // Create a main room sector
    CudaSector mainRoom;
    mainRoom.floorHeight = 0.0f;
    mainRoom.ceilingHeight = 2.0f;
    mainRoom.floorTextureId = 0;  // Assuming texture ID 0 exists
    mainRoom.ceilingTextureId = 1; // Assuming texture ID 1 exists
    mainRoom.lightLevel = 200;  // Bright room
    mainRoom.wallStartIndex = 0; // We'll set this after adding walls
    mainRoom.wallCount = 0;      // We'll set this after adding walls
    
    // Create a corridor sector
    CudaSector corridor;
    corridor.floorHeight = 0.0f;
    corridor.ceilingHeight = 1.5f; // Lower ceiling in the corridor
    corridor.floorTextureId = 0;
    corridor.ceilingTextureId = 1;
    corridor.lightLevel = 150;     // Dimmer corridor
    corridor.wallStartIndex = 0;   // We'll set this after adding walls
    corridor.wallCount = 0;        // We'll set this after adding walls
    
    // Create a small side room
    CudaSector sideRoom;
    sideRoom.floorHeight = 0.1f;   // Slightly elevated floor
    sideRoom.ceilingHeight = 1.8f;
    sideRoom.floorTextureId = 2;   // Different floor texture
    sideRoom.ceilingTextureId = 1;
    sideRoom.lightLevel = 100;     // Darker room
    sideRoom.wallStartIndex = 0;   // We'll set this after adding walls
    sideRoom.wallCount = 0;        // We'll set this after adding walls
    
    // Add sectors to our collection
    int mainRoomId = 0;
    sectors.push_back(mainRoom);
    
    int corridorId = 1;
    sectors.push_back(corridor);
    
    int sideRoomId = 2;
    sectors.push_back(sideRoom);
    
    // ======== DEFINE WALLS ========
    
    // Main room walls (5x5 square, centered at origin)
    // We'll make one wall a portal to the corridor
    
    // Track the starting wall index for each sector
    size_t mainRoomWallStart = walls.size();
    
    // Main room - North wall (with portal to corridor)
    CudaWall northWall1;
    northWall1.segment.start.x = -2.5f;
    northWall1.segment.start.y = 2.5f;
    northWall1.segment.end.x = -0.5f;
    northWall1.segment.end.y = 2.5f;
    northWall1.sectorFront = mainRoomId;
    northWall1.sectorBack = -1;
    northWall1.textureId = 3;
    northWall1.lightLevel = sectors[mainRoomId].lightLevel;
    walls.push_back(northWall1);
    
    // Portal to corridor
    CudaWall portalWall;
    portalWall.segment.start.x = -0.5f;
    portalWall.segment.start.y = 2.5f;
    portalWall.segment.end.x = 0.5f;
    portalWall.segment.end.y = 2.5f;
    portalWall.sectorFront = mainRoomId;
    portalWall.sectorBack = corridorId;
    portalWall.textureId = 4; // Portal texture
    portalWall.lightLevel = sectors[mainRoomId].lightLevel;
    walls.push_back(portalWall);
    
    // Main room - North wall continued
    CudaWall northWall2;
    northWall2.segment.start.x = 0.5f;
    northWall2.segment.start.y = 2.5f;
    northWall2.segment.end.x = 2.5f;
    northWall2.segment.end.y = 2.5f;
    northWall2.sectorFront = mainRoomId;
    northWall2.sectorBack = -1;
    northWall2.textureId = 3;
    northWall2.lightLevel = sectors[mainRoomId].lightLevel;
    walls.push_back(northWall2);
    
    // Main room - East wall
    CudaWall eastWall;
    eastWall.segment.start.x = 2.5f;
    eastWall.segment.start.y = 2.5f;
    eastWall.segment.end.x = 2.5f;
    eastWall.segment.end.y = -2.5f;
    eastWall.sectorFront = mainRoomId;
    eastWall.sectorBack = -1;
    eastWall.textureId = 3;
    eastWall.lightLevel = sectors[mainRoomId].lightLevel;
    walls.push_back(eastWall);
    
    // Main room - South wall
    CudaWall southWall;
    southWall.segment.start.x = 2.5f;
    southWall.segment.start.y = -2.5f;
    southWall.segment.end.x = -2.5f;
    southWall.segment.end.y = -2.5f;
    southWall.sectorFront = mainRoomId;
    southWall.sectorBack = -1;
    southWall.textureId = 3;
    southWall.lightLevel = sectors[mainRoomId].lightLevel;
    walls.push_back(southWall);
    
    // Main room - West wall
    CudaWall westWall;
    westWall.segment.start.x = -2.5f;
    westWall.segment.start.y = -2.5f;
    westWall.segment.end.x = -2.5f;
    westWall.segment.end.y = 2.5f;
    westWall.sectorFront = mainRoomId;
    westWall.sectorBack = -1;
    westWall.textureId = 3;
    westWall.lightLevel = sectors[mainRoomId].lightLevel;
    walls.push_back(westWall);
    
    // Update main room wall indices
    sectors[mainRoomId].wallStartIndex = mainRoomWallStart;
    sectors[mainRoomId].wallCount = walls.size() - mainRoomWallStart;
    
    // Corridor walls
    size_t corridorWallStart = walls.size();
    
    // Corridor - Connection to main room
    CudaWall corridorEntrance;
    corridorEntrance.segment.start.x = 0.5f;
    corridorEntrance.segment.start.y = 2.5f;
    corridorEntrance.segment.end.x = -0.5f;
    corridorEntrance.segment.end.y = 2.5f;
    corridorEntrance.sectorFront = corridorId;
    corridorEntrance.sectorBack = mainRoomId;
    corridorEntrance.textureId = 4; // Portal texture
    corridorEntrance.lightLevel = sectors[corridorId].lightLevel;
    walls.push_back(corridorEntrance);
    
    // Corridor - West wall
    CudaWall corridorWestWall;
    corridorWestWall.segment.start.x = -0.5f;
    corridorWestWall.segment.start.y = 2.5f;
    corridorWestWall.segment.end.x = -0.5f;
    corridorWestWall.segment.end.y = 4.5f;
    corridorWestWall.sectorFront = corridorId;
    corridorWestWall.sectorBack = -1;
    corridorWestWall.textureId = 5; // Different texture
    corridorWestWall.lightLevel = sectors[corridorId].lightLevel;
    walls.push_back(corridorWestWall);
    
    // Corridor - North wall (with portal to side room)
    CudaWall corridorNorthWall1;
    corridorNorthWall1.segment.start.x = -0.5f;
    corridorNorthWall1.segment.start.y = 4.5f;
    corridorNorthWall1.segment.end.x = -0.25f;
    corridorNorthWall1.segment.end.y = 4.5f;
    corridorNorthWall1.sectorFront = corridorId;
    corridorNorthWall1.sectorBack = -1;
    corridorNorthWall1.textureId = 5;
    corridorNorthWall1.lightLevel = sectors[corridorId].lightLevel;
    walls.push_back(corridorNorthWall1);
    
    // Portal to side room
    CudaWall sideRoomPortal;
    sideRoomPortal.segment.start.x = -0.25f;
    sideRoomPortal.segment.start.y = 4.5f;
    sideRoomPortal.segment.end.x = 0.25f;
    sideRoomPortal.segment.end.y = 4.5f;
    sideRoomPortal.sectorFront = corridorId;
    sideRoomPortal.sectorBack = sideRoomId;
    sideRoomPortal.textureId = 4; // Portal texture
    sideRoomPortal.lightLevel = sectors[corridorId].lightLevel;
    walls.push_back(sideRoomPortal);
    
    // Corridor - North wall continued
    CudaWall corridorNorthWall2;
    corridorNorthWall2.segment.start.x = 0.25f;
    corridorNorthWall2.segment.start.y = 4.5f;
    corridorNorthWall2.segment.end.x = 0.5f;
    corridorNorthWall2.segment.end.y = 4.5f;
    corridorNorthWall2.sectorFront = corridorId;
    corridorNorthWall2.sectorBack = -1;
    corridorNorthWall2.textureId = 5;
    corridorNorthWall2.lightLevel = sectors[corridorId].lightLevel;
    walls.push_back(corridorNorthWall2);
    
    // Corridor - East wall
    CudaWall corridorEastWall;
    corridorEastWall.segment.start.x = 0.5f;
    corridorEastWall.segment.start.y = 4.5f;
    corridorEastWall.segment.end.x = 0.5f;
    corridorEastWall.segment.end.y = 2.5f;
    corridorEastWall.sectorFront = corridorId;
    corridorEastWall.sectorBack = -1;
    corridorEastWall.textureId = 5;
    corridorEastWall.lightLevel = sectors[corridorId].lightLevel;
    walls.push_back(corridorEastWall);
    
    // Update corridor wall indices
    sectors[corridorId].wallStartIndex = corridorWallStart;
    sectors[corridorId].wallCount = walls.size() - corridorWallStart;
    
    // Side room walls
    size_t sideRoomWallStart = walls.size();
    
    // Side room - Connection to corridor
    CudaWall sideRoomEntrance;
    sideRoomEntrance.segment.start.x = 0.25f;
    sideRoomEntrance.segment.start.y = 4.5f;
    sideRoomEntrance.segment.end.x = -0.25f;
    sideRoomEntrance.segment.end.y = 4.5f;
    sideRoomEntrance.sectorFront = sideRoomId;
    sideRoomEntrance.sectorBack = corridorId;
    sideRoomEntrance.textureId = 4; // Portal texture
    sideRoomEntrance.lightLevel = sectors[sideRoomId].lightLevel;
    walls.push_back(sideRoomEntrance);
    
    // Side room - West wall
    CudaWall sideRoomWestWall;
    sideRoomWestWall.segment.start.x = -0.25f;
    sideRoomWestWall.segment.start.y = 4.5f;
    sideRoomWestWall.segment.end.x = -1.5f;
    sideRoomWestWall.segment.end.y = 5.5f;
    sideRoomWestWall.sectorFront = sideRoomId;
    sideRoomWestWall.sectorBack = -1;
    sideRoomWestWall.textureId = 6; // Different texture
    sideRoomWestWall.lightLevel = sectors[sideRoomId].lightLevel;
    walls.push_back(sideRoomWestWall);
    
    // Side room - North wall
    CudaWall sideRoomNorthWall;
    sideRoomNorthWall.segment.start.x = -1.5f;
    sideRoomNorthWall.segment.start.y = 5.5f;
    sideRoomNorthWall.segment.end.x = 1.5f;
    sideRoomNorthWall.segment.end.y = 5.5f;
    sideRoomNorthWall.sectorFront = sideRoomId;
    sideRoomNorthWall.sectorBack = -1;
    sideRoomNorthWall.textureId = 6;
    sideRoomNorthWall.lightLevel = sectors[sideRoomId].lightLevel;
    walls.push_back(sideRoomNorthWall);
    
    // Side room - East wall
    CudaWall sideRoomEastWall;
    sideRoomEastWall.segment.start.x = 1.5f;
    sideRoomEastWall.segment.start.y = 5.5f;
    sideRoomEastWall.segment.end.x = 0.25f;
    sideRoomEastWall.segment.end.y = 4.5f;
    sideRoomEastWall.sectorFront = sideRoomId;
    sideRoomEastWall.sectorBack = -1;
    sideRoomEastWall.textureId = 6;
    sideRoomEastWall.lightLevel = sectors[sideRoomId].lightLevel;
    walls.push_back(sideRoomEastWall);
    
    // Update side room wall indices
    sectors[sideRoomId].wallStartIndex = sideRoomWallStart;
    sectors[sideRoomId].wallCount = walls.size() - sideRoomWallStart;
    
    // ======== BUILD BSP TREE ========
    
    // Create the leaf node for each sector
    CudaBSPNode mainRoomNode;
    mainRoomNode.isLeaf = true;
    mainRoomNode.sectorId = mainRoomId;
    mainRoomNode.wallStartIndex = sectors[mainRoomId].wallStartIndex;
    mainRoomNode.wallCount = sectors[mainRoomId].wallCount;
    mainRoomNode.frontNodeIndex = -1;
    mainRoomNode.backNodeIndex = -1;
    
    CudaBSPNode corridorNode;
    corridorNode.isLeaf = true;
    corridorNode.sectorId = corridorId;
    corridorNode.wallStartIndex = sectors[corridorId].wallStartIndex;
    corridorNode.wallCount = sectors[corridorId].wallCount;
    corridorNode.frontNodeIndex = -1;
    corridorNode.backNodeIndex = -1;
    
    CudaBSPNode sideRoomNode;
    sideRoomNode.isLeaf = true;
    sideRoomNode.sectorId = sideRoomId;
    sideRoomNode.wallStartIndex = sectors[sideRoomId].wallStartIndex;
    sideRoomNode.wallCount = sectors[sideRoomId].wallCount;
    sideRoomNode.frontNodeIndex = -1;
    sideRoomNode.backNodeIndex = -1;
    
    // Create a binary partitioner that separates side room from main room+corridor
    CudaBSPNode partitioner1;
    partitioner1.isLeaf = false;
    partitioner1.sectorId = -1;
    partitioner1.wallStartIndex = -1;
    partitioner1.wallCount = 0;
    // Partition line that separates side room from the rest
    partitioner1.partitioner.start.x = -1.0f;
    partitioner1.partitioner.start.y = 4.0f;
    partitioner1.partitioner.end.x = 1.0f;
    partitioner1.partitioner.end.y = 4.0f;
    
    // Create a binary partitioner that separates main room from corridor
    CudaBSPNode partitioner2;
    partitioner2.isLeaf = false;
    partitioner2.sectorId = -1;
    partitioner2.wallStartIndex = -1;
    partitioner2.wallCount = 0;
    // Partition line that separates main room from corridor
    partitioner2.partitioner.start.x = -1.0f;
    partitioner2.partitioner.start.y = 2.0f;
    partitioner2.partitioner.end.x = 1.0f;
    partitioner2.partitioner.end.y = 2.0f;
    
    // Build the full BSP tree
    // Add root node (partitioner1)
    nodes.push_back(partitioner1);
    int rootNodeIndex = 0;
    
    // Add partitioner2 as front child of root
    nodes.push_back(partitioner2);
    nodes[rootNodeIndex].frontNodeIndex = 1;
    
    // Add side room as back child of root
    nodes.push_back(sideRoomNode);
    nodes[rootNodeIndex].backNodeIndex = 2;
    
    // Add main room as front child of partitioner2
    nodes.push_back(mainRoomNode);
    nodes[1].frontNodeIndex = 3;
    
    // Add corridor as back child of partitioner2
    nodes.push_back(corridorNode);
    nodes[1].backNodeIndex = 4;
    
    // ======== ALLOCATE CUDA MEMORY ========
    
    // Create host structure to hold our map data
    CudaBSPTree testMap;
    
    // Allocate device memory for nodes
    cudaError_t err = cudaMalloc((void**)&testMap.nodes, nodes.size() * sizeof(CudaBSPNode));
    if (err != cudaSuccess) {
        std::cerr << "Failed to allocate node memory: " << cudaGetErrorString(err) << std::endl;
        return testMap; // Return empty structure on error
    }
    
    // Allocate device memory for walls
    err = cudaMalloc((void**)&testMap.walls, walls.size() * sizeof(CudaWall));
    if (err != cudaSuccess) {
        cudaFree(testMap.nodes);
        std::cerr << "Failed to allocate wall memory: " << cudaGetErrorString(err) << std::endl;
        return testMap; // Return empty structure on error
    }
    
    // Allocate device memory for sectors
    err = cudaMalloc((void**)&testMap.sectors, sectors.size() * sizeof(CudaSector));
    if (err != cudaSuccess) {
        cudaFree(testMap.nodes);
        cudaFree(testMap.walls);
        std::cerr << "Failed to allocate sector memory: " << cudaGetErrorString(err) << std::endl;
        return testMap; // Return empty structure on error
    }
    
    // Copy data to device
    err = cudaMemcpy(testMap.nodes, nodes.data(), nodes.size() * sizeof(CudaBSPNode), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        cudaFree(testMap.nodes);
        cudaFree(testMap.walls);
        cudaFree(testMap.sectors);
        std::cerr << "Failed to copy nodes to device: " << cudaGetErrorString(err) << std::endl;
        return testMap; // Return empty structure on error
    }
    
    err = cudaMemcpy(testMap.walls, walls.data(), walls.size() * sizeof(CudaWall), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        cudaFree(testMap.nodes);
        cudaFree(testMap.walls);
        cudaFree(testMap.sectors);
        std::cerr << "Failed to copy walls to device: " << cudaGetErrorString(err) << std::endl;
        return testMap; // Return empty structure on error
    }
    
    err = cudaMemcpy(testMap.sectors, sectors.data(), sectors.size() * sizeof(CudaSector), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        cudaFree(testMap.nodes);
        cudaFree(testMap.walls);
        cudaFree(testMap.sectors);
        std::cerr << "Failed to copy sectors to device: " << cudaGetErrorString(err) << std::endl;
        return testMap; // Return empty structure on error
    }
    
    // Set the remaining properties
    testMap.nodeCount = nodes.size();
    testMap.wallCount = walls.size();
    testMap.sectorCount = sectors.size();
    testMap.rootNodeIndex = rootNodeIndex;
    
    std::cout << "Successfully created test map with:" << std::endl;
    std::cout << "  " << nodes.size() << " nodes" << std::endl;
    std::cout << "  " << walls.size() << " walls" << std::endl;
    std::cout << "  " << sectors.size() << " sectors" << std::endl;
    
    return testMap;
}

// Function to free the test map resources
void freeTestMap(CudaBSPTree& testMap) {
    if (testMap.nodes != nullptr) {
        cudaFree(testMap.nodes);
        testMap.nodes = nullptr;
    }
    
    if (testMap.walls != nullptr) {
        cudaFree(testMap.walls);
        testMap.walls = nullptr;
    }
    
    if (testMap.sectors != nullptr) {
        cudaFree(testMap.sectors);
        testMap.sectors = nullptr;
    }
    
    testMap.nodeCount = 0;
    testMap.wallCount = 0;
    testMap.sectorCount = 0;
    testMap.rootNodeIndex = 0;
}

} // namespace PureDoom 