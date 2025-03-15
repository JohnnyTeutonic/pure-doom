#include "BSPTree.h"
#include <iostream>
#include <vector>

using namespace PureDoom;

// Function to create a simple test map
std::vector<Sector> createTestMap() {
    std::vector<Sector> sectors;
    
    // Create a simple square room
    Sector room;
    room.floorHeight = 0.0f;
    room.ceilingHeight = 3.0f;
    room.floorTextureId = 1;
    room.ceilingTextureId = 2;
    room.lightLevel = 128;
    
    // Four walls forming a square room
    room.walls.push_back(Wall(Line(Vertex(0.0f, 0.0f), Vertex(10.0f, 0.0f)), 0, -1, 3));
    room.walls.push_back(Wall(Line(Vertex(10.0f, 0.0f), Vertex(10.0f, 10.0f)), 0, -1, 4));
    room.walls.push_back(Wall(Line(Vertex(10.0f, 10.0f), Vertex(0.0f, 10.0f)), 0, -1, 5));
    room.walls.push_back(Wall(Line(Vertex(0.0f, 10.0f), Vertex(0.0f, 0.0f)), 0, -1, 6));
    
    sectors.push_back(room);
    
    // Create a second room connected to the first
    Sector secondRoom;
    secondRoom.floorHeight = 0.0f;
    secondRoom.ceilingHeight = 3.0f;
    secondRoom.floorTextureId = 7;
    secondRoom.ceilingTextureId = 8;
    secondRoom.lightLevel = 200;
    
    // Four walls forming the second room
    secondRoom.walls.push_back(Wall(Line(Vertex(10.0f, 0.0f), Vertex(20.0f, 0.0f)), 1, -1, 9));
    secondRoom.walls.push_back(Wall(Line(Vertex(20.0f, 0.0f), Vertex(20.0f, 10.0f)), 1, -1, 10));
    secondRoom.walls.push_back(Wall(Line(Vertex(20.0f, 10.0f), Vertex(10.0f, 10.0f)), 1, -1, 11));
    
    // Portal wall connecting to the first room (note: two-way portal)
    secondRoom.walls.push_back(Wall(Line(Vertex(10.0f, 10.0f), Vertex(10.0f, 0.0f)), 1, 0, 12));
    
    // Update the corresponding wall in the first room to be a portal as well
    room.walls[1].sectorBack = 1;
    
    sectors[0] = room;
    sectors.push_back(secondRoom);
    
    return sectors;
}

// Function to test ray casting
void testRayCasting(const BSPTree& bsp) {
    std::cout << "\n--- Ray Casting Test ---\n";
    
    // Test rays from different positions
    struct RayTest {
        Vec2 origin;
        Vec2 direction;
        float maxDistance;
        std::string description;
    };
    
    std::vector<RayTest> tests = {
        {Vec2(5.0f, 5.0f), Vec2(1.0f, 0.0f), 20.0f, "From center of room 1 toward room 2"},
        {Vec2(5.0f, 5.0f), Vec2(0.0f, 1.0f), 20.0f, "From center of room 1 toward north wall"},
        {Vec2(15.0f, 5.0f), Vec2(-1.0f, 0.0f), 20.0f, "From center of room 2 toward room 1"},
        {Vec2(15.0f, 5.0f), Vec2(1.0f, 0.0f), 20.0f, "From center of room 2 toward east wall"}
    };
    
    for (const auto& test : tests) {
        Vec2 hitPoint;
        int hitWallIndex;
        
        bool hit = bsp.traceRay(test.origin, test.direction, test.maxDistance, hitPoint, hitWallIndex);
        
        std::cout << "Ray test: " << test.description << "\n";
        std::cout << "  Origin: (" << test.origin.x << ", " << test.origin.y << ")\n";
        std::cout << "  Direction: (" << test.direction.x << ", " << test.direction.y << ")\n";
        
        if (hit) {
            std::cout << "  Hit at point: (" << hitPoint.x << ", " << hitPoint.y << ")\n";
            std::cout << "  Hit wall index: " << hitWallIndex << "\n";
        } else {
            std::cout << "  No hit detected.\n";
        }
        
        std::cout << "\n";
    }
}

int main() {
    std::cout << "PureDoom - BSP Tree Implementation\n";
    std::cout << "==================================\n\n";
    
    // Create a test map
    std::vector<Sector> testMap = createTestMap();
    
    std::cout << "Test map created with " << testMap.size() << " sectors:\n";
    for (size_t i = 0; i < testMap.size(); ++i) {
        const Sector& sector = testMap[i];
        std::cout << "Sector " << i << " has " << sector.walls.size() << " walls\n";
    }
    
    // Create and build the BSP tree
    BSPTree bsp;
    bsp.build(testMap);
    
    // Test rendering from a viewpoint
    std::cout << "\n--- Rendering Test ---\n";
    Vec2 viewPos(5.0f, 5.0f);
    float viewAngle = 0.0f;  // Looking east
    float fov = 90.0f;       // 90 degree field of view
    
    bsp.render(viewPos, viewAngle, fov);
    
    // Test ray casting
    testRayCasting(bsp);
    
    std::cout << "\nBSP tree tests completed successfully.\n";
    
    return 0;
} 