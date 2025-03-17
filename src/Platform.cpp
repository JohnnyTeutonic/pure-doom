#include "Platform.h"
#include "BSPTree.h" // Now we can include BSPTree.h here

namespace PureDoom {

// Check if a point is inside the platform (2D check)
bool Platform::containsPoint(const Vec2& point) const {
    if (vertices.size() < 3) return false;
    
    // Ray casting algorithm to determine if point is inside polygon
    bool inside = false;
    size_t j = vertices.size() - 1;
    
    for (size_t i = 0; i < vertices.size(); i++) {
        if (((vertices[i].y > point.y) != (vertices[j].y > point.y)) &&
            (point.x < (vertices[j].x - vertices[i].x) * (point.y - vertices[i].y) / 
            (vertices[j].y - vertices[i].y) + vertices[i].x)) {
            inside = !inside;
        }
        j = i;
    }
    
    return inside;
}

// Get the normal vector for a platform edge
Vec2 Platform::getEdgeNormal(size_t edgeIndex) const {
    if (vertices.size() < 2) return Vec2(0, 0);
    
    size_t nextIndex = (edgeIndex + 1) % vertices.size();
    Vec2 edge = vertices[nextIndex] - vertices[edgeIndex];
    
    // Return the normalized perpendicular vector (counter-clockwise rotation)
    return Vec2(-edge.y, edge.x).normalized();
}

// Update the platform's state (for moving platforms)
void Platform::update(float deltaTime) {
    if (!isMoving) return;
    
    // Calculate the height change for this frame
    float heightChange = moveSpeed * deltaTime;
    
    // Determine direction of movement
    if (height < targetHeight) {
        // Moving up
        height = std::min(height + heightChange, targetHeight);
        if (height >= targetHeight) {
            isMoving = false;
        }
    } else if (height > targetHeight) {
        // Moving down
        height = std::max(height - heightChange, targetHeight);
        if (height <= targetHeight) {
            isMoving = false;
        }
    } else {
        // Already at target height
        isMoving = false;
    }
}

// Trigger the platform (for interactive platforms)
void Platform::trigger() {
    if (isTriggered) return;
    
    isTriggered = true;
    
    switch (type) {
        case PlatformType::ELEVATOR:
            // Toggle between current height and target height
            {
                float temp = height;
                height = targetHeight;
                targetHeight = temp;
                isMoving = true;
            }
            break;
            
        case PlatformType::DOOR:
            // If door is closed (at floor level), open it
            if (height <= 0.1f) {
                targetHeight = 2.0f; // Open height
            } else {
                targetHeight = 0.0f; // Closed height
            }
            isMoving = true;
            break;
            
        case PlatformType::CRUSHER:
            // Start crushing motion
            targetHeight = 0.1f; // Almost to floor level
            isMoving = true;
            break;
            
        default:
            // No special behavior for other platform types
            break;
    }
}

// Create a stair platform
Platform Platform::createStair(const Vec2& start, const Vec2& end, float baseHeight, 
                             float stepHeight, int index, int count, 
                             int topTex, int bottomTex, int sideTex, int light, int sector) {
    // Calculate the step dimensions
    Vec2 direction = (end - start).normalized();
    float stepLength = (end - start).length() / count;
    
    // Calculate the step position
    Vec2 stepStart = start + direction * (index * stepLength);
    Vec2 stepEnd = start + direction * ((index + 1) * stepLength);
    
    // Create perpendicular vector for step width
    Vec2 perpendicular(-direction.y, direction.x);
    float stepWidth = 1.5f; // Increased step width for better visibility (was 1.0f)
    
    // Create the four corners of the step
    std::vector<Vec2> vertices;
    vertices.push_back(stepStart + perpendicular * (stepWidth / 2.0f));
    vertices.push_back(stepEnd + perpendicular * (stepWidth / 2.0f));
    vertices.push_back(stepEnd - perpendicular * (stepWidth / 2.0f));
    vertices.push_back(stepStart - perpendicular * (stepWidth / 2.0f));
    
    // Calculate the height for this step
    float height = baseHeight + (index * stepHeight);
    
    // Create the platform
    Platform platform(vertices, height, stepHeight, topTex, bottomTex, sideTex, light, sector);
    
    // Set stair-specific properties
    platform.type = PlatformType::STAIR;
    platform.stairIndex = index;
    platform.stairCount = count;
    platform.tag = "stair_" + std::to_string(index);
    
    // Make sure the platform is visible and solid
    platform.isVisible = true;
    platform.isSolid = true;
    
    return platform;
}

} // namespace PureDoom 