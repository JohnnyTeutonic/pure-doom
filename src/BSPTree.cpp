#include "BSPTree.h"
#include <algorithm>
#include <limits>
#include <iostream>

namespace PureDoom {

// Constructor and destructor
BSPTree::BSPTree() : m_root(nullptr) {}

BSPTree::~BSPTree() = default;

// Build the BSP tree from a list of sectors
void BSPTree::build(const std::vector<Sector>& sectors) {
    m_sectors = sectors;
    
    // Collect all walls from all sectors
    std::vector<Wall> allWalls;
    for (size_t i = 0; i < sectors.size(); ++i) {
        const Sector& sector = sectors[i];
        for (const Wall& wall : sector.walls) {
            Wall wallCopy = wall;
            if (wallCopy.sectorFront == -1) {
                wallCopy.sectorFront = static_cast<int>(i);
            }
            allWalls.push_back(wallCopy);
        }
    }
    
    // Build the tree recursively
    m_root = buildTree(std::move(allWalls));
    
    std::cout << "BSP Tree built successfully with " << sectors.size() << " sectors.\n";
}

// Recursive function to build the BSP tree
std::unique_ptr<BSPNode> BSPTree::buildTree(std::vector<Wall> walls) {
    // If no walls are left, return nullptr (empty space)
    if (walls.empty()) {
        return nullptr;
    }
    
    // If only one wall is left, create a leaf node
    if (walls.size() == 1) {
        auto node = std::make_unique<BSPNode>();
        node->isLeaf = true;
        node->walls = std::move(walls);
        node->sectorId = walls[0].sectorFront;
        return node;
    }
    
    // Find the best splitter among the walls
    Line splitter = findBestSplitter(walls);
    
    // Create the node with this splitter
    auto node = std::make_unique<BSPNode>(splitter);
    
    // Classify and distribute walls to front and back
    std::vector<Wall> frontWalls;
    std::vector<Wall> backWalls;
    
    for (const Wall& wall : walls) {
        SplitType splitType = classifyWall(wall, splitter);
        
        switch (splitType) {
            case SplitType::FRONT:
                frontWalls.push_back(wall);
                break;
                
            case SplitType::BACK:
                backWalls.push_back(wall);
                break;
                
            case SplitType::SPANNING: {
                // Wall spans the splitter - split it into two
                Wall frontPart, backPart;
                splitWall(wall, splitter, frontPart, backPart);
                frontWalls.push_back(frontPart);
                backWalls.push_back(backPart);
                break;
            }
            
            case SplitType::COLINEAR:
                // Add to front side by convention
                frontWalls.push_back(wall);
                break;
        }
    }
    
    // Recursively build the front and back subtrees
    node->front = buildTree(std::move(frontWalls));
    node->back = buildTree(std::move(backWalls));
    
    return node;
}

// Find the best splitter for a set of walls
Line BSPTree::findBestSplitter(const std::vector<Wall>& walls) const {
    if (walls.empty()) {
        // Fallback - should never happen if called properly
        return Line(0.0f, 0.0f, 1.0f, 0.0f);
    }
    
    // Simple heuristic: Use the first wall
    // In a production system, you'd want a more sophisticated heuristic
    // that minimizes the number of splits
    
    // More advanced heuristic (commented out for simplicity):
    /*
    int bestScore = std::numeric_limits<int>::max();
    size_t bestIndex = 0;
    
    for (size_t i = 0; i < walls.size(); ++i) {
        const Line& candidate = walls[i].segment;
        int splits = 0;
        int balance = 0;
        
        for (const Wall& wall : walls) {
            if (&wall.segment == &candidate) continue;
            
            SplitType type = classifyWall(wall, candidate);
            if (type == SplitType::FRONT) balance++;
            else if (type == SplitType::BACK) balance--;
            else if (type == SplitType::SPANNING) splits++;
        }
        
        // Calculate score: minimize splits and balance the tree
        int score = 10 * splits + std::abs(balance);
        if (score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }
    
    return walls[bestIndex].segment;
    */
    
    return walls[0].segment;
}

// Classify a wall with respect to a partitioner line
SplitType BSPTree::classifyWall(const Wall& wall, const Line& partitioner) const {
    // Compute which side of the partitioner the start and end points lie on
    Vec2 partDir = partitioner.direction();
    Vec2 normal(-partDir.y, partDir.x);  // Normal vector to the partitioner
    
    Vec2 startToPartStart = wall.segment.start.position - partitioner.start.position;
    Vec2 endToPartStart = wall.segment.end.position - partitioner.start.position;
    
    float startSide = normal.dotProduct(startToPartStart);
    float endSide = normal.dotProduct(endToPartStart);
    
    // Check classification based on point positions
    if (std::abs(startSide) < 0.0001f && std::abs(endSide) < 0.0001f) {
        // Both points lie on the partitioner
        return SplitType::COLINEAR;
    } else if (startSide >= 0.0f && endSide >= 0.0f) {
        // Both points are in front of the partitioner
        return SplitType::FRONT;
    } else if (startSide <= 0.0f && endSide <= 0.0f) {
        // Both points are behind the partitioner
        return SplitType::BACK;
    } else {
        // The line spans across the partitioner
        return SplitType::SPANNING;
    }
}

// Split a wall that spans across a partitioner
void BSPTree::splitWall(const Wall& wall, const Line& partitioner, 
                      Wall& frontWall, Wall& backWall) const {
    // Calculate intersection point
    Vec2 p1 = wall.segment.start.position;
    Vec2 p2 = wall.segment.end.position;
    Vec2 p3 = partitioner.start.position;
    Vec2 p4 = partitioner.end.position;
    
    // Line equations:
    // p1 + t * (p2 - p1) = p3 + s * (p4 - p3)
    // Solve for t
    
    Vec2 v1 = p2 - p1;
    Vec2 v2 = p4 - p3;
    Vec2 v3 = p1 - p3;
    
    float cross = v1.crossProduct(v2);
    if (std::abs(cross) < 0.0001f) {
        // Lines are nearly parallel, just split in the middle
        Vec2 mid = (p1 + p2) * 0.5f;
        
        frontWall = Wall(Line(p1, mid), wall.sectorFront, wall.sectorBack, wall.textureId);
        backWall = Wall(Line(mid, p2), wall.sectorFront, wall.sectorBack, wall.textureId);
        return;
    }
    
    float t = v2.crossProduct(v3) / cross;
    Vec2 intersection = p1 + v1 * t;
    
    // Compute which side of the partitioner the start and end points lie on
    Vec2 partDir = partitioner.direction();
    Vec2 normal(-partDir.y, partDir.x);  // Normal vector to the partitioner
    
    Vec2 startToPartStart = p1 - p3;
    Vec2 endToPartStart = p2 - p3;
    
    float startSide = normal.dotProduct(startToPartStart);
    float endSide = normal.dotProduct(endToPartStart);
    
    if (startSide >= 0.0f) {
        // Start is in front, end is in back
        frontWall = Wall(Line(p1, intersection), wall.sectorFront, wall.sectorBack, wall.textureId);
        backWall = Wall(Line(intersection, p2), wall.sectorFront, wall.sectorBack, wall.textureId);
    } else {
        // Start is in back, end is in front
        backWall = Wall(Line(p1, intersection), wall.sectorFront, wall.sectorBack, wall.textureId);
        frontWall = Wall(Line(intersection, p2), wall.sectorFront, wall.sectorBack, wall.textureId);
    }
}

// Trace a ray through the BSP tree
bool BSPTree::traceRay(const Vec2& origin, const Vec2& direction, float maxDistance, 
                       Vec2& hitPoint, int& hitWallIndex) const {
    if (!m_root) {
        return false;
    }
    
    return traceRayRecursive(m_root.get(), origin, direction, maxDistance, hitPoint, hitWallIndex);
}

// Recursive function to trace a ray through the BSP tree
bool BSPTree::traceRayRecursive(const BSPNode* node, const Vec2& origin, const Vec2& direction,
                               float maxDistance, Vec2& hitPoint, int& hitWallIndex) const {
    if (!node) {
        return false;
    }
    
    // If this is a leaf node, check for intersection with all walls
    if (node->isLeaf) {
        bool hit = false;
        float closestDist = maxDistance;
        
        for (size_t i = 0; i < node->walls.size(); ++i) {
            const Wall& wall = node->walls[i];
            
            Vec2 wallStart = wall.segment.start.position;
            Vec2 wallEnd = wall.segment.end.position;
            
            // Simple line segment intersection
            Vec2 v1 = origin - wallStart;
            Vec2 v2 = wallEnd - wallStart;
            Vec2 v3(-direction.y, direction.x);
            
            float dot = v2.dotProduct(v3);
            if (std::abs(dot) < 0.0001f) {
                // Lines are parallel
                continue;
            }
            
            float t1 = v2.crossProduct(v1) / dot;
            float t2 = v1.dotProduct(v3) / dot;
            
            if (t1 >= 0.0f && t1 < closestDist && t2 >= 0.0f && t2 <= 1.0f) {
                // Hit!
                closestDist = t1;
                hitPoint = origin + direction * t1;
                hitWallIndex = static_cast<int>(i);
                hit = true;
            }
        }
        
        return hit;
    }
    
    // Otherwise, traverse the BSP tree
    Vec2 partDir = node->partitioner.direction();
    Vec2 normal(-partDir.y, partDir.x);  // Normal vector to the partitioner
    
    Vec2 toPartStart = origin - node->partitioner.start.position;
    float side = normal.dotProduct(toPartStart);
    float dirSide = normal.dotProduct(direction);
    
    // Determine which side(s) to check
    if (side >= 0.0f) {
        // Origin is in front of the partitioner
        
        // Check front side first
        if (traceRayRecursive(node->front.get(), origin, direction, maxDistance, hitPoint, hitWallIndex)) {
            return true;
        }
        
        // If the ray is pointing to the back side, check it too
        if (dirSide < 0.0f) {
            // Calculate distance to partitioner
            float t = -side / dirSide;
            if (t < maxDistance) {
                // Compute new origin and continue from there
                Vec2 newOrigin = origin + direction * t;
                float newMaxDistance = maxDistance - t;
                
                return traceRayRecursive(node->back.get(), newOrigin, direction, newMaxDistance, hitPoint, hitWallIndex);
            }
        }
    } else {
        // Origin is behind the partitioner
        
        // Check back side first
        if (traceRayRecursive(node->back.get(), origin, direction, maxDistance, hitPoint, hitWallIndex)) {
            return true;
        }
        
        // If the ray is pointing to the front side, check it too
        if (dirSide > 0.0f) {
            // Calculate distance to partitioner
            float t = -side / dirSide;
            if (t < maxDistance) {
                // Compute new origin and continue from there
                Vec2 newOrigin = origin + direction * t;
                float newMaxDistance = maxDistance - t;
                
                return traceRayRecursive(node->front.get(), newOrigin, direction, newMaxDistance, hitPoint, hitWallIndex);
            }
        }
    }
    
    return false;
}

// Render the scene from a viewpoint
void BSPTree::render(const Vec2& viewPosition, float viewAngle, float fov) const {
    if (!m_root) {
        return;
    }
    
    // This function would typically interface with a rendering engine
    // For demonstration purposes, we'll just print some information
    std::cout << "Rendering from position (" << viewPosition.x << ", " << viewPosition.y << ") "
              << "at angle " << viewAngle << " with FOV " << fov << std::endl;
    
    // In an actual implementation, we would traverse the BSP tree in back-to-front order
    // and render each node's walls with proper perspective projection
    
    // Find which sector the viewer is in
    int sectorId = findSector(viewPosition);
    if (sectorId >= 0) {
        std::cout << "Viewer is in sector " << sectorId << std::endl;
    } else {
        std::cout << "Viewer is not in any sector" << std::endl;
    }
    
    // Perform a back-to-front traversal to render the scene
    renderRecursive(m_root.get(), viewPosition, viewAngle, fov);
}

// Recursive function to render the scene
void BSPTree::renderRecursive(const BSPNode* node, const Vec2& viewPosition, 
                            float viewAngle, float fov) const {
    if (!node) {
        return;
    }
    
    // Determine which side of the partition the viewer is on
    Vec2 partDir = node->partitioner.direction();
    Vec2 normal(-partDir.y, partDir.x);  // Normal vector to the partitioner
    
    Vec2 toPartStart = viewPosition - node->partitioner.start.position;
    float side = normal.dotProduct(toPartStart);
    
    if (side >= 0.0f) {
        // Viewer is in front of the partitioner
        // Render back side first, then front side
        renderRecursive(node->back.get(), viewPosition, viewAngle, fov);
        
        // If this is a leaf node, render its walls
        if (node->isLeaf) {
            for (const Wall& wall : node->walls) {
                // In a real renderer, this would draw the wall with texture
                // Here we just print some information
                std::cout << "Rendering wall: (" 
                          << wall.segment.start.position.x << ", " << wall.segment.start.position.y << ") to ("
                          << wall.segment.end.position.x << ", " << wall.segment.end.position.y << ")" << std::endl;
            }
        }
        
        renderRecursive(node->front.get(), viewPosition, viewAngle, fov);
    } else {
        // Viewer is behind the partitioner
        // Render front side first, then back side
        renderRecursive(node->front.get(), viewPosition, viewAngle, fov);
        
        // If this is a leaf node, render its walls
        if (node->isLeaf) {
            for (const Wall& wall : node->walls) {
                // In a real renderer, this would draw the wall with texture
                // Here we just print some information
                std::cout << "Rendering wall: (" 
                          << wall.segment.start.position.x << ", " << wall.segment.start.position.y << ") to ("
                          << wall.segment.end.position.x << ", " << wall.segment.end.position.y << ")" << std::endl;
            }
        }
        
        renderRecursive(node->back.get(), viewPosition, viewAngle, fov);
    }
}

// Check if a point is inside a sector
int BSPTree::findSector(const Vec2& point) const {
    if (!m_root) {
        return -1;
    }
    
    return findSectorRecursive(m_root.get(), point);
}

// Recursive function to find which sector contains a point
int BSPTree::findSectorRecursive(const BSPNode* node, const Vec2& point) const {
    if (!node) {
        return -1;
    }
    
    if (node->isLeaf) {
        return node->sectorId;
    }
    
    // Determine which side of the partition the point is on
    Vec2 partDir = node->partitioner.direction();
    Vec2 normal(-partDir.y, partDir.x);  // Normal vector to the partitioner
    
    Vec2 toPartStart = point - node->partitioner.start.position;
    float side = normal.dotProduct(toPartStart);
    
    if (side >= 0.0f) {
        // Point is in front of the partitioner
        return findSectorRecursive(node->front.get(), point);
    } else {
        // Point is behind the partitioner
        return findSectorRecursive(node->back.get(), point);
    }
}

} // namespace PureDoom 