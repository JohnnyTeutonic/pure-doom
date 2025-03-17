#ifndef PLATFORM_H
#define PLATFORM_H

#include <vector>
#include <memory>
#include <string>
#include <cmath>

namespace PureDoom {

// Vec2 definition to avoid circular dependency
struct Vec2 {
    float x, y;
    
    Vec2() : x(0.0f), y(0.0f) {}
    Vec2(float x, float y) : x(x), y(y) {}
    
    Vec2 operator+(const Vec2& other) const {
        return Vec2(x + other.x, y + other.y);
    }
    
    Vec2 operator-(const Vec2& other) const {
        return Vec2(x - other.x, y - other.y);
    }
    
    Vec2 operator*(float scalar) const {
        return Vec2(x * scalar, y * scalar);
    }
    
    float length() const {
        return std::sqrt(x * x + y * y);
    }
    
    float lengthSquared() const {
        return x * x + y * y;
    }
    
    Vec2 normalized() const {
        float len = length();
        if (len < 0.0001f) return Vec2();
        return Vec2(x / len, y / len);
    }
    
    float dotProduct(const Vec2& other) const {
        return x * other.x + y * other.y;
    }
    
    float crossProduct(const Vec2& other) const {
        return x * other.y - y * other.x;
    }
    
    // Distance to another point
    float distanceTo(const Vec2& other) const {
        return (*this - other).length();
    }
    
    // Check if two points are approximately equal
    bool approxEquals(const Vec2& other, float epsilon = 0.0001f) const {
        return std::abs(x - other.x) < epsilon && std::abs(y - other.y) < epsilon;
    }
};

// Platform types for different behaviors
enum class PlatformType {
    STATIC,         // Non-moving platform
    ELEVATOR,       // Platform that moves up and down
    DOOR,           // Platform that acts as a door
    STAIR,          // Platform that forms a stair step
    CRUSHER         // Platform that can crush the player
};

// Structure to represent an elevated platform in the game
struct Platform {
    std::vector<Vec2> vertices;     // Vertices defining the platform shape (in counter-clockwise order)
    float height;                   // Height of the platform above the floor
    float thickness;                // Thickness of the platform
    int topTextureId;               // Texture ID for the top surface
    int bottomTextureId;            // Texture ID for the bottom surface
    int sideTextureId;              // Texture ID for the sides
    int lightLevel;                 // Light level for the platform
    int sectorId;                   // Sector ID where the platform is located
    std::string tag;                // Optional tag for identifying the platform
    
    // New properties for enhanced functionality
    PlatformType type;              // Type of platform
    bool isSolid;                   // Whether the platform blocks movement
    bool isVisible;                 // Whether the platform is visible
    
    // Movement properties
    float targetHeight;             // Target height for moving platforms
    float moveSpeed;                // Speed of movement
    bool isMoving;                  // Whether the platform is currently moving
    bool isTriggered;               // Whether the platform has been triggered
    std::string triggerTag;         // Tag that triggers this platform
    
    // Stair properties
    int stairIndex;                 // Index in a stair sequence (0 = bottom)
    int stairCount;                 // Total number of stairs in the sequence
    
    // Constructor with default values
    Platform() : 
        height(1.0f),
        thickness(0.2f),
        topTextureId(-1),
        bottomTextureId(-1),
        sideTextureId(-1),
        lightLevel(255),
        sectorId(-1),
        type(PlatformType::STATIC),
        isSolid(true),
        isVisible(true),
        targetHeight(1.0f),
        moveSpeed(0.0f),
        isMoving(false),
        isTriggered(false),
        stairIndex(0),
        stairCount(1) {}
    
    // Constructor with parameters
    Platform(const std::vector<Vec2>& verts, float h, float t, int topTex, int bottomTex, int sideTex, int light, int sector) :
        vertices(verts),
        height(h),
        thickness(t),
        topTextureId(topTex),
        bottomTextureId(bottomTex),
        sideTextureId(sideTex),
        lightLevel(light),
        sectorId(sector),
        type(PlatformType::STATIC),
        isSolid(true),
        isVisible(true),
        targetHeight(h),
        moveSpeed(0.0f),
        isMoving(false),
        isTriggered(false),
        stairIndex(0),
        stairCount(1) {}
    
    // Check if a point is inside the platform (2D check)
    bool containsPoint(const Vec2& point) const;
    
    // Get the normal vector for a platform edge
    Vec2 getEdgeNormal(size_t edgeIndex) const;
    
    // Get the world height of the top surface
    float getTopHeight() const {
        return height;
    }
    
    // Get the world height of the bottom surface
    float getBottomHeight() const {
        return height - thickness;
    }
    
    // Update the platform's state (for moving platforms)
    void update(float deltaTime);
    
    // Trigger the platform (for interactive platforms)
    void trigger();
    
    // Create a stair platform
    static Platform createStair(const Vec2& start, const Vec2& end, float baseHeight, 
                               float stepHeight, int index, int count, 
                               int topTex, int bottomTex, int sideTex, int light, int sector);
};

} // namespace PureDoom

#endif // PLATFORM_H 