// Platform class inline implementation

// Default constructor
inline Platform::Platform() :
    height(0.0f),
    thickness(0.15f),
    topTextureId(0),
    bottomTextureId(0),
    sideTextureId(0),
    lightLevel(255),
    sectorId(0),
    type(PlatformType::STATIC),
    isVisible(true),
    isSolid(true),
    isMoving(false),
    targetHeight(0.0f),
    moveSpeed(0.0f),
    tag("platform")
{
}

// Constructor with parameters
inline Platform::Platform(const std::vector<Vec2>& verts, float h, float t,
                         int topTex, int bottomTex, int sideTex,
                         int light, int sector) :
    vertices(verts),
    height(h),
    thickness(t),
    topTextureId(topTex),
    bottomTextureId(bottomTex),
    sideTextureId(sideTex),
    lightLevel(light),
    sectorId(sector),
    type(PlatformType::STATIC),
    isVisible(true),
    isSolid(true),
    isMoving(false),
    targetHeight(h),
    moveSpeed(0.0f),
    tag("platform")
{
}

// Get the top height of the platform
inline float Platform::getTopHeight() const {
    return height + thickness;
}

// Get the bottom height of the platform
inline float Platform::getBottomHeight() const {
    return height;
}

// Check if a point is inside the platform (2D check)
inline bool Platform::containsPoint(const Vec2& point) const {
    if (vertices.size() < 3) return false;
    
    bool inside = false;
    int j = vertices.size() - 1;
    
    for (int i = 0; i < vertices.size(); i++) {
        if (((vertices[i].y > point.y) != (vertices[j].y > point.y)) &&
            (point.x < (vertices[j].x - vertices[i].x) * (point.y - vertices[i].y) / 
             (vertices[j].y - vertices[i].y) + vertices[i].x)) {
            inside = !inside;
        }
        j = i;
    }
    
    return inside;
}

// Update the platform (for moving platforms)
inline void Platform::update(float deltaTime) {
    if (!isMoving) return;
    
    // Simple linear movement between current height and target height
    float heightDiff = targetHeight - height;
    
    if (std::abs(heightDiff) < 0.01f) {
        // We're close enough to target, stop moving
        height = targetHeight;
        isMoving = false;
        return;
    }
    
    // Move towards the target height
    float moveAmount = moveSpeed * deltaTime;
    
    if (heightDiff > 0) {
        // Moving up
        height = std::min(height + moveAmount, targetHeight);
    } else {
        // Moving down
        height = std::max(height - moveAmount, targetHeight);
    }
}

// Get the normal vector for a platform edge
inline Vec2 Platform::getEdgeNormal(size_t edgeIndex) const {
    // Make sure the platform has enough vertices
    if (vertices.size() < 3) {
        return Vec2(0.0f, 0.0f);
    }
    
    // Make sure the edge index is valid
    size_t numEdges = vertices.size();
    if (edgeIndex >= numEdges) {
        return Vec2(0.0f, 0.0f);
    }
    
    // Get the vertices of the edge
    size_t nextIndex = (edgeIndex + 1) % numEdges;
    const Vec2& start = vertices[edgeIndex];
    const Vec2& end = vertices[nextIndex];
    
    // Calculate the edge direction vector
    Vec2 edgeDir = end - start;
    
    // Calculate the normal (perpendicular to the edge, pointing outward)
    // For a counter-clockwise vertex ordering, the normal is (-dy, dx)
    Vec2 normal(-edgeDir.y, edgeDir.x);
    
    // Normalize the normal vector
    return normal.normalized();
}

// Trigger the platform (for interactive platforms)
inline void Platform::trigger() {
    // Skip if already moving
    if (!isMoving) {
        isMoving = true;
    }
    
    switch (type) {
        case PlatformType::ELEVATOR:
            // Toggle between current height and target height
            {
                float temp = height;
                targetHeight = temp > 0.1f ? 0.0f : 1.0f; // Toggle between up/down
                moveSpeed = 0.5f; // Set a reasonable speed
            }
            break;
            
        case PlatformType::TRIGGER:
            // Start moving to the target height
            targetHeight = height > 0.1f ? 0.0f : 0.3f; // Toggle position
            moveSpeed = 0.3f; // Set a reasonable speed
            break;
            
        default:
            // No special behavior for other platform types
            break;
    }
}

// Static function to create a stair element (DOOM-style)
inline Platform Platform::createStair(const Vec2& start, const Vec2& end, 
                                    float baseHeight, float stepHeight, 
                                    int stepIndex, int totalSteps,
                                    int topTextureId, int bottomTextureId, int sideTextureId,
                                    int lightLevel, int sectorId) {
    // Create a stair platform with proper dimensions
    std::vector<Vec2> stairVertices;
    
    // Calculate stair width (distance from start to end)
    float stairWidth = (end - start).length();
    
    // Calculate depth/extension of each stair
    float stairDepth = 0.5f;
    
    // Calculate step height
    float currentHeight = baseHeight + (stepIndex * stepHeight);
    
    // Set thickness to lower value for thin DOOM-style platforms
    float thickness = 0.1f;
    
    // Calculate direction from start to end
    Vec2 direction = (end - start).normalized();
    
    // Calculate perpendicular direction for the stair depth (extending backward)
    Vec2 perpendicular(-direction.y, direction.x);
    
    // For DOOM style stairs, extend the stair backward to create a true platform feeling
    Vec2 startBack = start - perpendicular * stairDepth;
    Vec2 endBack = end - perpendicular * stairDepth;
    
    // Define vertices in counter-clockwise order
    stairVertices.push_back(start);     // Front start
    stairVertices.push_back(end);       // Front end
    stairVertices.push_back(endBack);   // Back end
    stairVertices.push_back(startBack); // Back start
    
    // Create the platform
    Platform stair(stairVertices, currentHeight, thickness,
                  topTextureId, bottomTextureId, sideTextureId, 
                  lightLevel, sectorId);
    
    // Set stair-specific properties
    stair.type = PlatformType::STAIR;
    stair.tag = "stair_" + std::to_string(stepIndex);
    
    return stair;
}

// Static function to create a room platform (DOOM-style sector)
inline Platform Platform::createRoomPlatform(float width, float length, float height,
                                           int topTextureId, int bottomTextureId, int sideTextureId,
                                           int lightLevel, int sectorId) {
    // Create a rectangular platform with the specified dimensions
    std::vector<Vec2> platformVertices;
    
    // Define vertices in counter-clockwise order (DOOM uses squared/rectangular sectors)
    platformVertices.push_back(Vec2(-width/2, -length/2)); // Bottom-left
    platformVertices.push_back(Vec2(width/2, -length/2));  // Bottom-right
    platformVertices.push_back(Vec2(width/2, length/2));   // Top-right
    platformVertices.push_back(Vec2(-width/2, length/2));  // Top-left
    
    // Create the platform with thin thickness
    Platform platform(platformVertices, height, 0.1f,
                     topTextureId, bottomTextureId, sideTextureId,
                     lightLevel, sectorId);
    
    // Set sector-specific properties
    platform.type = PlatformType::SECTOR;
    platform.tag = "sector_platform";
    platform.isVisible = true;
    platform.isSolid = true;
    
    return platform;
} 