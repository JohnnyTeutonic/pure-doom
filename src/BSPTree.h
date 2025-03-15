#ifndef BSP_TREE_H
#define BSP_TREE_H

#include <vector>
#include <memory>
#include <cmath>
#include <string>
#include <map>
#include <unordered_map>
#include <functional>
#include <limits>

namespace PureDoom {

// Forward declarations
struct Vertex;
struct Line;
struct Sector;
struct Wall;
struct BSPNode;

// 2D vector for coordinates
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

// Vertex representation
struct Vertex {
    Vec2 position;
    
    Vertex() = default;
    Vertex(float x, float y) : position(x, y) {}
    Vertex(const Vec2& pos) : position(pos) {}
    
    bool approxEquals(const Vertex& other, float epsilon = 0.0001f) const {
        return position.approxEquals(other.position, epsilon);
    }
};

// Line segment representation
struct Line {
    Vertex start;
    Vertex end;
    
    Line() = default;
    Line(const Vertex& s, const Vertex& e) : start(s), end(e) {}
    Line(float x1, float y1, float x2, float y2) : start(x1, y1), end(x2, y2) {}
    
    Vec2 direction() const {
        return (end.position - start.position).normalized();
    }
    
    float length() const {
        return (end.position - start.position).length();
    }
    
    // Check if a point lies on this line segment
    bool containsPoint(const Vec2& point, float epsilon = 0.0001f) const {
        if (length() < epsilon) {
            // Line is essentially a point, check if point matches start/end
            return point.approxEquals(start.position, epsilon);
        }
        
        Vec2 d1 = point - start.position;
        Vec2 d2 = end.position - start.position;
        
        // Check if vectors are parallel (cross product ≈ 0)
        if (std::abs(d1.crossProduct(d2)) > epsilon * d2.length()) {
            return false; // Not on the line
        }
        
        // Check if point is within the line segment bounds
        float t = d1.dotProduct(d2) / d2.lengthSquared();
        return t >= 0.0f && t <= 1.0f;
    }
    
    // Calculate the shortest distance from a point to this line segment
    float distanceToPoint(const Vec2& point) const {
        Vec2 v = end.position - start.position;
        Vec2 w = point - start.position;
        
        float c1 = w.dotProduct(v);
        if (c1 <= 0.0f) {
            return w.length(); // Point is before start
        }
        
        float c2 = v.lengthSquared();
        if (c2 <= c1) {
            return (point - end.position).length(); // Point is after end
        }
        
        float b = c1 / c2;
        Vec2 pb = start.position + v * b;
        return (point - pb).length(); // Distance to projection
    }
};

// Wall representation with enhanced attributes
struct Wall {
    Line segment;
    int sectorFront;     // Index of sector in front of the wall
    int sectorBack;      // Index of sector behind the wall (-1 if solid)
    int textureId;       // Texture index for rendering
    
    // New attributes for enhanced functionality
    float textureOffsetX;  // Horizontal texture offset
    float textureOffsetY;  // Vertical texture offset
    bool isSolid;          // Whether the wall blocks movement
    bool isTransparent;    // Whether the wall is see-through
    std::string tag;       // Optional tag for identifying special walls
    
    Wall() : sectorFront(-1), sectorBack(-1), textureId(-1),
             textureOffsetX(0.0f), textureOffsetY(0.0f),
             isSolid(true), isTransparent(false) {}
    
    Wall(const Line& line, int front, int back = -1, int texture = -1)
        : segment(line), sectorFront(front), sectorBack(back), textureId(texture),
          textureOffsetX(0.0f), textureOffsetY(0.0f),
          isSolid(true), isTransparent(false) {}
        
    bool isPortal() const {
        return sectorBack != -1;
    }
    
    // Check if two walls are approximately equal
    bool approxEquals(const Wall& other, float epsilon = 0.0001f) const {
        return segment.start.approxEquals(other.segment.start, epsilon) &&
               segment.end.approxEquals(other.segment.end, epsilon) &&
               sectorFront == other.sectorFront &&
               sectorBack == other.sectorBack;
    }
};

// Special sector properties for enhanced functionality
enum class SectorType {
    NORMAL,     // Standard sector
    DOOR,       // Door that can open/close
    ELEVATOR,   // Platform that can move up/down
    CRUSHER,    // Ceiling that can crush the player
    DAMAGING,   // Sector that causes damage (like lava)
    SPECIAL     // Triggers special effects
};

// Define sector movement types
enum class MovementType {
    NONE,           // No movement
    LINEAR,         // Linear movement
    SINE_WAVE,      // Sine wave movement
    TRIGGERED_ONCE, // Triggered once and stops
    TRIGGERED_LOOP  // Triggered and loops
};

// Sector representation with enhanced properties
struct Sector {
    std::vector<Wall> walls;
    float floorHeight;
    float ceilingHeight;
    int floorTextureId;
    int ceilingTextureId;
    int lightLevel;
    
    // New attributes for enhanced functionality
    SectorType type;             // Type of sector
    std::string tag;             // Optional tag for identifying special sectors
    
    // Movement properties
    MovementType floorMovement;  // Type of floor movement
    MovementType ceilingMovement;// Type of ceiling movement
    float movementSpeed;         // Speed of movement
    float movementDistance;      // Distance to move
    float movementProgress;      // Current progress (0.0 to 1.0)
    bool movementActive;         // Whether movement is currently active
    
    // Trigger info
    std::string triggerTag;      // Tag that triggers this sector
    bool triggeredOnce;          // Whether it has been triggered
    
    // Default sector
    Sector() : floorHeight(0.0f), ceilingHeight(0.0f), 
               floorTextureId(-1), ceilingTextureId(-1), lightLevel(0),
               type(SectorType::NORMAL),
               floorMovement(MovementType::NONE), ceilingMovement(MovementType::NONE),
               movementSpeed(0.0f), movementDistance(0.0f),
               movementProgress(0.0f), movementActive(false),
               triggeredOnce(false) {}
               
    // Check if this is a moving sector
    bool isMoving() const {
        return floorMovement != MovementType::NONE || ceilingMovement != MovementType::NONE;
    }
    
    // Update moving sectors
    void update(float deltaTime) {
        if (!movementActive) return;
        
        float previousProgress = movementProgress;
        
        switch (floorMovement) {
            case MovementType::LINEAR:
                movementProgress += deltaTime * movementSpeed;
                break;
            case MovementType::SINE_WAVE:
                movementProgress += deltaTime * movementSpeed;
                // Keep it cycling between 0 and 1
                movementProgress = fmodf(movementProgress, 1.0f);
                break;
            case MovementType::TRIGGERED_ONCE:
                if (movementProgress < 1.0f) {
                    movementProgress += deltaTime * movementSpeed;
                    if (movementProgress >= 1.0f) {
                        movementProgress = 1.0f;
                        if (floorMovement == MovementType::TRIGGERED_ONCE) {
                            movementActive = false;
                        }
                    }
                }
                break;
            case MovementType::TRIGGERED_LOOP:
                movementProgress += deltaTime * movementSpeed;
                if (movementProgress >= 1.0f) {
                    movementProgress = 0.0f;
                }
                break;
            default:
                break;
        }
        
        // Apply movement to sector heights
        if (floorMovement != MovementType::NONE) {
            float factor = 0.0f;
            if (floorMovement == MovementType::SINE_WAVE) {
                factor = (std::sin(movementProgress * 6.28318f) + 1.0f) * 0.5f;
            } else {
                factor = movementProgress;
            }
            floorHeight = floorHeight + (movementDistance * (factor - (previousProgress < 0.0001f ? 0.0f : previousProgress)));
        }
        
        // Same for ceiling if it's moving
        if (ceilingMovement != MovementType::NONE) {
            // Implementation similar to floor movement
        }
    }
    
    // Trigger the sector movement
    void trigger() {
        if (!triggeredOnce || 
            floorMovement == MovementType::TRIGGERED_LOOP || 
            ceilingMovement == MovementType::TRIGGERED_LOOP) {
            movementActive = true;
            triggeredOnce = true;
        }
    }
};

// BSP Node for the tree
struct BSPNode {
    Line partitioner;             // The line used to split space
    std::unique_ptr<BSPNode> front; // Node representing space in front of partitioner
    std::unique_ptr<BSPNode> back;  // Node representing space behind partitioner
    std::vector<Wall> walls;      // Walls contained in this node (leaf only)
    bool isLeaf;                 // Flag indicating if this is a leaf node
    int sectorId;                // Index of sector this node belongs to (leaf only)
    
    BSPNode() : isLeaf(false), sectorId(-1) {}
    
    explicit BSPNode(const Line& line) 
        : partitioner(line), isLeaf(false), sectorId(-1) {}
};

// Split classification
enum class SplitType {
    FRONT,    // Completely in front of the partitioner
    BACK,     // Completely behind the partitioner
    SPANNING, // Spans across both sides of the partitioner
    COLINEAR  // Lies on the partitioner
};

// Collision information
struct CollisionInfo {
    bool collision;        // Whether a collision occurred
    Vec2 point;            // Point of collision
    Vec2 normal;           // Surface normal at collision point
    int wallIndex;         // Index of the wall hit
    int sectorId;          // Sector where collision occurred
    float distance;        // Distance to collision
    
    CollisionInfo() : collision(false), wallIndex(-1), sectorId(-1), distance(std::numeric_limits<float>::max()) {}
};

// The main BSP Tree class with enhanced functionality
class BSPTree {
public:
    BSPTree();
    ~BSPTree();
    
    // Build BSP tree from a list of sectors
    void build(const std::vector<Sector>& sectors);
    
    // Trace a ray through the BSP tree
    bool traceRay(const Vec2& origin, const Vec2& direction, float maxDistance, 
                  Vec2& hitPoint, int& hitWallIndex) const;
    
    // Enhanced ray casting with more detailed collision info
    CollisionInfo castRay(const Vec2& origin, const Vec2& direction, float maxDistance) const;
    
    // Collision detection for moving objects (returns collision info)
    CollisionInfo checkCollision(const Vec2& position, float radius, const Vec2& velocity) const;
    
    // Render the scene from a viewpoint
    void render(const Vec2& viewPosition, float viewAngle, float fov) const;
    
    // Check if a point is inside a sector
    int findSector(const Vec2& point) const;
    
    // Get all sectors in the scene
    const std::vector<Sector>& getSectors() const { return m_sectors; }
    
    // Update moving sectors
    void update(float deltaTime);
    
    // Trigger a sector by tag
    void triggerSector(const std::string& tag);
    
    // Check if a sector is visible from a viewpoint
    bool isSectorVisible(int sectorId, const Vec2& viewPosition, float viewAngle, float fov) const;
    
    // Find potential portals visible from a viewpoint
    std::vector<int> findVisiblePortals(int startSectorId, const Vec2& viewPosition, 
                                        float viewAngle, float fov) const;
                                        
    // Helper method for findVisiblePortals with recursion control
    std::vector<int> findVisiblePortalsRecursive(int currentSectorId, const Vec2& viewPosition,
                                              float viewAngle, float fov,
                                              std::unordered_map<int, bool>& processedSectors,
                                              int currentDepth, int maxDepth) const;
                                        
    // Validate the BSP tree (for debugging)
    bool validate() const;
    
private:
    std::unique_ptr<BSPNode> m_root; // Root of the BSP tree
    std::vector<Sector> m_sectors;   // All sectors in the scene
    
    // Mapping of tags to sector indices for quick lookup
    std::unordered_map<std::string, std::vector<int>> m_tagToSectors;
    
    // Cache for portal visibility
    mutable std::map<std::pair<int, int>, bool> m_portalVisibilityCache;
    
    // Recursive function to build the BSP tree
    std::unique_ptr<BSPNode> buildTree(std::vector<Wall> walls);
    
    // Find the best splitter for a set of walls
    Line findBestSplitter(const std::vector<Wall>& walls) const;
    
    // Classify a wall with respect to a partitioner line
    SplitType classifyWall(const Wall& wall, const Line& partitioner) const;
    
    // Split a wall that spans across a partitioner
    void splitWall(const Wall& wall, const Line& partitioner, 
                   Wall& frontWall, Wall& backWall) const;
                   
    // Check for convexity in a set of walls
    bool isConvex(const std::vector<Wall>& walls) const;
    
    // Optimize a set of walls by merging colinear segments
    std::vector<Wall> optimizeWalls(const std::vector<Wall>& walls) const;
    
    // Recursive function to trace a ray through the BSP tree
    bool traceRayRecursive(const BSPNode* node, const Vec2& origin, const Vec2& direction,
                          float maxDistance, Vec2& hitPoint, int& hitWallIndex) const;
                          
    // Enhanced ray casting recursive function
    void castRayRecursive(const BSPNode* node, const Vec2& origin, const Vec2& direction,
                         float maxDistance, CollisionInfo& collision) const;
                         
    // Recursive function to check for collisions
    void checkCollisionRecursive(const BSPNode* node, const Vec2& position, float radius, 
                               const Vec2& velocity, CollisionInfo& collision) const;
                          
    // Recursive function to render the scene
    void renderRecursive(const BSPNode* node, const Vec2& viewPosition, 
                         float viewAngle, float fov) const;
                         
    // Recursive function to find which sector contains a point
    int findSectorRecursive(const BSPNode* node, const Vec2& point) const;
    
    // Recursive function to validate the BSP tree
    bool validateRecursive(const BSPNode* node) const;
    
    // Check if a portal is potentially visible from a viewpoint
    bool isPortalVisible(const Wall& portalWall, const Vec2& viewPosition, 
                        float viewAngle, float fov) const;
};

} // namespace PureDoom

#endif // BSP_TREE_H 