#include "Platform.h"
#include "BSPTree.h" // Now we can include BSPTree.h here
#include <iostream>  // Add missing include for std::cout and std::endl

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
    // Calculate the position and dimensions of this step
    float stepWidth = (end - start).length();
    float stepDepth = 1.0f;  // Increased depth of each step for better collision detection
    
    // Calculate the vertices for this step (more precise)
    std::vector<Vec2> vertices;
    
    // Calculate step height (bottom to top stair)
    float height = baseHeight + (index * stepHeight);
    
    // Direction of the step
    Vec2 stepDir = (end - start).normalized();
    Vec2 stepNormal(-stepDir.y, stepDir.x);
    
    // Calculate step positions - make each step overlap slightly with the next
    float stepLength = stepWidth / count;
    float overlap = 0.05f; // Slight overlap between steps
    
    // Start position for this step (ensure first step connects with ground level)
    float startPos;
    if (index == 0) {
        // First step should start exactly at the specified start position
        startPos = 0;
    } else {
        // Subsequent steps should overlap slightly with the previous step
        startPos = (stepLength * index) - overlap;
    }
    
    // End position for this step (ensure last step reaches the top)
    float endPos;
    if (index == count - 1) {
        // Last step should end exactly at the specified end position
        endPos = stepWidth;
    } else {
        // Intermediate steps should overlap slightly with the next step
        endPos = (stepLength * (index + 1)) + overlap;
    }
    
    // Calculate the actual start and end points
    Vec2 stepStart = start + stepDir * startPos;
    Vec2 stepEnd = start + stepDir * endPos;
    
    // Make stairs wider by extending them to the sides
    float sideExtension = 0.5f; // How much to extend on each side
    
    // Add vertices for the step (counter-clockwise ordering)
    // Front edge (extended)
    vertices.push_back(stepStart - stepNormal * (stepDepth / 2 + sideExtension));
    vertices.push_back(stepEnd - stepNormal * (stepDepth / 2 + sideExtension));
    
    // Back edge (extended)
    vertices.push_back(stepEnd + stepNormal * (stepDepth / 2 + sideExtension));
    vertices.push_back(stepStart + stepNormal * (stepDepth / 2 + sideExtension));
    
    // Create the platform with solid collision
    Platform platform(vertices, height, stepHeight, topTex, bottomTex, sideTex, light, sector);
    
    // Set stair-specific properties
    platform.type = PlatformType::STAIR;
    platform.stairIndex = index;
    platform.stairCount = count;
    platform.tag = "stair_" + std::to_string(index);
    
    // Make sure the platform is visible and solid
    platform.isVisible = true;
    platform.isSolid = true;
    
    // Debug output
    std::cout << "Created stair platform " << index + 1 << " of " << count 
              << " at height " << height 
              << ", start: (" << stepStart.x << ", " << stepStart.y << ")"
              << ", end: (" << stepEnd.x << ", " << stepEnd.y << ")" << std::endl;
    
    return platform;
}

} // namespace PureDoom 