#include "BSPTree.h"
#include <algorithm>
#include <limits>
#include <iostream>
#include <stdexcept>

namespace PureDoom {

// Constructor and destructor
BSPTree::BSPTree() : m_root(nullptr) {}

BSPTree::~BSPTree() = default;

// Build the BSP tree from a list of sectors
void BSPTree::build(const std::vector<Sector>& sectors) {
    std::cout << "Building BSP tree with " << sectors.size() << " sectors..." << std::endl;
    m_sectors = sectors;
    
    // Collect all walls from all sectors
    std::vector<Wall> allWalls;
    for (size_t i = 0; i < sectors.size(); ++i) {
        const Sector& sector = sectors[i];
        std::cout << "Processing sector " << i << " with " << sector.walls.size() << " walls..." << std::endl;
        
        for (const Wall& wall : sector.walls) {
            Wall wallCopy = wall;
            if (wallCopy.sectorFront == -1) {
                wallCopy.sectorFront = static_cast<int>(i);
            }
            allWalls.push_back(wallCopy);
        }
    }
    
    std::cout << "Collected " << allWalls.size() << " walls for BSP construction." << std::endl;
    
    // Build the tree recursively
    try {
        m_root = buildTree(std::move(allWalls));
        std::cout << "BSP Tree built successfully with " << sectors.size() << " sectors.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during BSP construction: " << e.what() << std::endl;
        throw;
    }
    catch (...) {
        std::cerr << "Unknown exception during BSP construction!" << std::endl;
        throw;
    }
}

// Recursive function to build the BSP tree
std::unique_ptr<BSPNode> BSPTree::buildTree(std::vector<Wall> walls) {
    // If no walls are left, return nullptr (empty space)
    if (walls.empty()) {
        std::cout << "No walls left, returning nullptr." << std::endl;
        return nullptr;
    }
    
    // If only one wall is left, create a leaf node
    if (walls.size() == 1) {
        std::cout << "Single wall, creating leaf node with sector ID: " << walls[0].sectorFront << std::endl;
        auto node = std::make_unique<BSPNode>();
        node->isLeaf = true;
        node->sectorId = walls[0].sectorFront;
        node->walls.push_back(walls[0]); // Copy the wall instead of moving the entire vector
        return node;
    }
    
    // Find the best splitter among the walls
    std::cout << "Finding best splitter among " << walls.size() << " walls..." << std::endl;
    Line splitter = findBestSplitter(walls);
    
    // Create the node with this splitter
    auto node = std::make_unique<BSPNode>(splitter);
    
    // Classify and distribute walls to front and back
    std::vector<Wall> frontWalls;
    std::vector<Wall> backWalls;
    
    std::cout << "Classifying walls..." << std::endl;
    for (const Wall& wall : walls) {
        SplitType splitType = classifyWall(wall, splitter);
        
        switch (splitType) {
            case SplitType::FRONT:
                std::cout << "Wall classified as FRONT" << std::endl;
                frontWalls.push_back(wall);
                break;
                
            case SplitType::BACK:
                std::cout << "Wall classified as BACK" << std::endl;
                backWalls.push_back(wall);
                break;
                
            case SplitType::SPANNING: {
                std::cout << "Wall classified as SPANNING, splitting..." << std::endl;
                // Wall spans the splitter - split it into two
                Wall frontPart, backPart;
                splitWall(wall, splitter, frontPart, backPart);
                frontWalls.push_back(frontPart);
                backWalls.push_back(backPart);
                break;
            }
            
            case SplitType::COLINEAR:
                std::cout << "Wall classified as COLINEAR" << std::endl;
                // Add to front side by convention
                frontWalls.push_back(wall);
                break;
        }
    }
    
    std::cout << "Walls distributed: " << frontWalls.size() << " front, " << backWalls.size() << " back" << std::endl;
    
    // Check if we're making progress in splitting the walls
    if ((frontWalls.size() == walls.size() && backWalls.empty()) || 
        (backWalls.size() == walls.size() && frontWalls.empty())) {
        std::cout << "Warning: No progress in splitting walls. Using different approach." << std::endl;
        
        // If we're not making progress, just make a leaf node with all walls
        auto leafNode = std::make_unique<BSPNode>();
        leafNode->isLeaf = true;
        
        // Try to find a common sector for all walls
        int commonSector = -1;
        for (const Wall& wall : walls) {
            if (commonSector == -1) {
                commonSector = wall.sectorFront;
            } else if (commonSector != wall.sectorFront) {
                // If walls belong to different sectors, use the first one
                std::cout << "Warning: Walls belong to different sectors in leaf node." << std::endl;
                break;
            }
        }
        
        leafNode->sectorId = (commonSector != -1) ? commonSector : walls[0].sectorFront;
        leafNode->walls = walls; // Copy all walls
        
        return leafNode;
    }
    
    // Recursively build the front and back subtrees
    std::cout << "Building front subtree..." << std::endl;
    if (!frontWalls.empty()) {
        node->front = buildTree(std::move(frontWalls));
    }
    
    std::cout << "Building back subtree..." << std::endl;
    if (!backWalls.empty()) {
        node->back = buildTree(std::move(backWalls));
    }
    
    return node;
}

// Find the best splitter for a set of walls
Line BSPTree::findBestSplitter(const std::vector<Wall>& walls) const {
    if (walls.empty()) {
        std::cerr << "Error: findBestSplitter called with empty walls vector" << std::endl;
        throw std::runtime_error("Cannot find splitter in empty walls vector");
    }
    
    // Use more advanced heuristic to find a good splitter
    int bestScore = std::numeric_limits<int>::max();
    size_t bestIndex = 0;
    
    for (size_t i = 0; i < walls.size(); ++i) {
        const Line& candidate = walls[i].segment;
        int splits = 0;
        int balance = 0;
        
        for (size_t j = 0; j < walls.size(); ++j) {
            if (i == j) continue; // Skip the candidate itself
            
            SplitType type = classifyWall(walls[j], candidate);
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
    
    const float EPSILON = 0.0001f;
    
    // Check classification based on point positions
    if (std::abs(startSide) < EPSILON && std::abs(endSide) < EPSILON) {
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
    
    // Clamp t to [0,1] to ensure the intersection point is on the wall segment
    t = std::max(0.0f, std::min(1.0f, t));
    
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
        
        // Check if front node exists before recursion
        if (node->front) {
            // Check front side first
            if (traceRayRecursive(node->front.get(), origin, direction, maxDistance, hitPoint, hitWallIndex)) {
                return true;
            }
        }
        
        // If the ray is pointing to the back side, check it too (if back node exists)
        if (dirSide < 0.0f && node->back) {
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
        
        // Check if back node exists before recursion
        if (node->back) {
            // Check back side first
            if (traceRayRecursive(node->back.get(), origin, direction, maxDistance, hitPoint, hitWallIndex)) {
                return true;
            }
        }
        
        // If the ray is pointing to the front side, check it too (if front node exists)
        if (dirSide > 0.0f && node->front) {
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
        std::cout << "No BSP tree to render!" << std::endl;
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
    
    // If this is a leaf node, render its walls
    if (node->isLeaf) {
        std::cout << "Rendering leaf node with sector ID: " << node->sectorId << std::endl;
        for (const Wall& wall : node->walls) {
            // In a real renderer, this would draw the wall with texture
            // Here we just print some information
            std::cout << "  Rendering wall: (" 
                      << wall.segment.start.position.x << ", " << wall.segment.start.position.y << ") to ("
                      << wall.segment.end.position.x << ", " << wall.segment.end.position.y << ")" << std::endl;
        }
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
        if (node->back) {
            renderRecursive(node->back.get(), viewPosition, viewAngle, fov);
        }
        
        if (node->front) {
            renderRecursive(node->front.get(), viewPosition, viewAngle, fov);
        }
    } else {
        // Viewer is behind the partitioner
        // Render front side first, then back side
        if (node->front) {
            renderRecursive(node->front.get(), viewPosition, viewAngle, fov);
        }
        
        if (node->back) {
            renderRecursive(node->back.get(), viewPosition, viewAngle, fov);
        }
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
        if (node->front) {
            return findSectorRecursive(node->front.get(), point);
        }
    } else {
        // Point is behind the partitioner
        if (node->back) {
            return findSectorRecursive(node->back.get(), point);
        }
    }
    
    // If we get here, the point is on one side but that subtree is missing
    // Try the other side as a fallback
    if (side >= 0.0f && node->back) {
        return findSectorRecursive(node->back.get(), point);
    } else if (side < 0.0f && node->front) {
        return findSectorRecursive(node->front.get(), point);
    }
    
    return -1;
}

} // namespace PureDoom 