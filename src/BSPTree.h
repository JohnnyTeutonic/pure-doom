#ifndef BSP_TREE_H
#define BSP_TREE_H

#include <vector>
#include <memory>
#include <cmath>

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
};

// Vertex representation
struct Vertex {
    Vec2 position;
    
    Vertex() = default;
    Vertex(float x, float y) : position(x, y) {}
    Vertex(const Vec2& pos) : position(pos) {}
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
};

// Wall representation
struct Wall {
    Line segment;
    int sectorFront;  // Index of sector in front of the wall
    int sectorBack;   // Index of sector behind the wall (-1 if solid)
    int textureId;    // Texture index for rendering
    
    Wall() : sectorFront(-1), sectorBack(-1), textureId(-1) {}
    
    Wall(const Line& line, int front, int back = -1, int texture = -1)
        : segment(line), sectorFront(front), sectorBack(back), textureId(texture) {}
        
    bool isPortal() const {
        return sectorBack != -1;
    }
};

// Sector representation
struct Sector {
    std::vector<Wall> walls;
    float floorHeight;
    float ceilingHeight;
    int floorTextureId;
    int ceilingTextureId;
    int lightLevel;
    
    Sector() : floorHeight(0.0f), ceilingHeight(0.0f), 
               floorTextureId(-1), ceilingTextureId(-1), lightLevel(0) {}
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
    FRONT,  // Completely in front of the partitioner
    BACK,   // Completely behind the partitioner
    SPANNING, // Spans across both sides of the partitioner
    COLINEAR  // Lies on the partitioner
};

// The main BSP Tree class
class BSPTree {
public:
    BSPTree();
    ~BSPTree();
    
    // Build BSP tree from a list of sectors
    void build(const std::vector<Sector>& sectors);
    
    // Trace a ray through the BSP tree
    bool traceRay(const Vec2& origin, const Vec2& direction, float maxDistance, 
                  Vec2& hitPoint, int& hitWallIndex) const;
    
    // Render the scene from a viewpoint
    void render(const Vec2& viewPosition, float viewAngle, float fov) const;
    
    // Check if a point is inside a sector
    int findSector(const Vec2& point) const;
    
    // Get all sectors in the scene
    const std::vector<Sector>& getSectors() const { return m_sectors; }
    
private:
    std::unique_ptr<BSPNode> m_root; // Root of the BSP tree
    std::vector<Sector> m_sectors;   // All sectors in the scene
    
    // Recursive function to build the BSP tree
    std::unique_ptr<BSPNode> buildTree(std::vector<Wall> walls);
    
    // Find the best splitter for a set of walls
    Line findBestSplitter(const std::vector<Wall>& walls) const;
    
    // Classify a wall with respect to a partitioner line
    SplitType classifyWall(const Wall& wall, const Line& partitioner) const;
    
    // Split a wall that spans across a partitioner
    void splitWall(const Wall& wall, const Line& partitioner, 
                   Wall& frontWall, Wall& backWall) const;
    
    // Recursive function to trace a ray through the BSP tree
    bool traceRayRecursive(const BSPNode* node, const Vec2& origin, const Vec2& direction,
                          float maxDistance, Vec2& hitPoint, int& hitWallIndex) const;
                          
    // Recursive function to render the scene
    void renderRecursive(const BSPNode* node, const Vec2& viewPosition, 
                         float viewAngle, float fov) const;
                         
    // Recursive function to find which sector contains a point
    int findSectorRecursive(const BSPNode* node, const Vec2& point) const;
};

} // namespace PureDoom

#endif // BSP_TREE_H 