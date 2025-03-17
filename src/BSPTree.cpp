#include "BSPTree.h"
#include <algorithm>
#include <limits>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

namespace PureDoom {

// Constructor and destructor
BSPTree::BSPTree() : m_root(nullptr) {}

BSPTree::~BSPTree() = default;

// Build the BSP tree from a list of sectors
void BSPTree::build(const std::vector<Sector>& sectors) {
    //std::cout << "Building BSP tree with " << sectors.size() << " sectors..." << std::endl;
    m_sectors = sectors;
    
    // Clear and populate the tag-to-sectors map
    m_tagToSectors.clear();
    for (size_t i = 0; i < sectors.size(); ++i) {
        const Sector& sector = sectors[i];
        if (!sector.tag.empty()) {
            m_tagToSectors[sector.tag].push_back(static_cast<int>(i));
        }
    }
    
    // Collect all walls from all sectors
    std::vector<Wall> allWalls;
    for (size_t i = 0; i < sectors.size(); ++i) {
        const Sector& sector = sectors[i];
        //std::cout << "Processing sector " << i << " with " << sector.walls.size() << " walls..." << std::endl;
        
        for (const Wall& wall : sector.walls) {
            Wall wallCopy = wall;
            if (wallCopy.sectorFront == -1) {
                wallCopy.sectorFront = static_cast<int>(i);
            }
            allWalls.push_back(wallCopy);
        }
    }
    
    //std::cout << "Collected " << allWalls.size() << " walls for BSP construction." << std::endl;
    
    // Build the tree recursively
    try {
        m_root = buildTree(std::move(allWalls));
        //std::cout << "BSP Tree built successfully with " << sectors.size() << " sectors.\n";
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
    static int buildDepth = 0;
    buildDepth++;
    
    
    // If no walls are left, return nullptr (empty space)
    if (walls.empty()) {
        buildDepth--;
        return nullptr;
    }
    
    // If only one wall is left, create a leaf node
    if (walls.size() == 1) {
        auto node = std::make_unique<BSPNode>();
        node->isLeaf = true;
        node->sectorId = walls[0].sectorFront;
        node->walls.push_back(walls[0]); // Copy the wall instead of moving the entire vector
        buildDepth--;
        return node;
    }
    
    // Find the best splitter among the walls
    Line splitter = findBestSplitter(walls);
    
    // Create the node with this splitter
    auto node = std::make_unique<BSPNode>(splitter);
    
    // Classify and distribute walls to front and back
    std::vector<Wall> frontWalls;
    std::vector<Wall> backWalls;
    
    //std::cout << "Classifying walls..." << std::endl;
    for (const Wall& wall : walls) {
        SplitType splitType = classifyWall(wall, splitter);
        
        switch (splitType) {
            case SplitType::FRONT:
                //std::cout << "Wall classified as FRONT" << std::endl;
                frontWalls.push_back(wall);
                break;
                
            case SplitType::BACK:
                //std::cout << "Wall classified as BACK" << std::endl;
                backWalls.push_back(wall);
                break;
                
            case SplitType::SPANNING: {
                //std::cout << "Wall classified as SPANNING, splitting..." << std::endl;
                // Wall spans the splitter - split it into two
                Wall frontPart, backPart;
                splitWall(wall, splitter, frontPart, backPart);
                frontWalls.push_back(frontPart);
                backWalls.push_back(backPart);
                break;
            }
            
            case SplitType::COLINEAR:
                //std::cout << "Wall classified as COLINEAR" << std::endl;
                // Add to front side by convention
                frontWalls.push_back(wall);
                break;
        }
    }
    
    //std::cout << "Walls distributed: " << frontWalls.size() << " front, " << backWalls.size() << " back" << std::endl;
    
    // Check if we're making progress in splitting the walls
    if ((frontWalls.size() == walls.size() && backWalls.empty()) || 
        (backWalls.size() == walls.size() && frontWalls.empty())) {
        
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
                break;
            }
        }
        
        leafNode->sectorId = (commonSector != -1) ? commonSector : walls[0].sectorFront;
        leafNode->walls = walls; // Copy all walls
        
        buildDepth--;
        return leafNode;
    }
    
    // Recursively build the front and back subtrees
    if (!frontWalls.empty()) {
        node->front = buildTree(std::move(frontWalls));
    }
    
    if (!backWalls.empty()) {
        node->back = buildTree(std::move(backWalls));
    }
    
    buildDepth--;
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
        int portalsFront = 0;
        int portalsBack = 0;
        
        for (size_t j = 0; j < walls.size(); ++j) {
            if (i == j) continue; // Skip the candidate itself
            
            const Wall& wall = walls[j];
            SplitType type = classifyWall(wall, candidate);
            
            if (type == SplitType::FRONT) {
                balance++;
                if (wall.isPortal()) portalsFront++;
            }
            else if (type == SplitType::BACK) {
                balance--;
                if (wall.isPortal()) portalsBack++;
            }
            else if (type == SplitType::SPANNING) {
                splits++;
            }
        }
        
        // Calculate score: minimize splits, balance the tree, and try to keep portals on one side
        int portalSplit = std::abs(portalsFront - portalsBack);
        int score = 10 * splits + std::abs(balance) + 5 * portalSplit;
        
        // Prefer using portal walls as splitters if possible
        if (walls[i].isPortal()) {
            score -= 5;
        }
        
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

// Enhanced ray casting with more detailed collision info
CollisionInfo BSPTree::castRay(const Vec2& origin, const Vec2& direction, float maxDistance) const {
    CollisionInfo collision;
    
    if (!m_root) {
        return collision;
    }
    
    // Initialize collision info
    collision.collision = false;
    collision.distance = maxDistance;
    
    // Cast the ray recursively through the BSP tree
    castRayRecursive(m_root.get(), origin, direction, maxDistance, collision);
    
    return collision;
}

// Enhanced ray casting recursive function
void BSPTree::castRayRecursive(const BSPNode* node, const Vec2& origin, const Vec2& direction, 
                             float maxDistance, CollisionInfo& collision) const {
    if (!node) {
        return;
    }
    
    // If this is a leaf node, check collision with all walls
    if (node->isLeaf) {
        // If the sector ID is invalid, there's nothing to check
        if (node->sectorId < 0 || node->sectorId >= static_cast<int>(m_sectors.size())) {
            return;
        }
        
        const Sector& sector = m_sectors[node->sectorId];
        
        // Find closest wall intersection in this sector
        float closestDist = maxDistance;
        bool hit = false;
        Vec2 hitPoint;
        int hitWallIndex = -1;
        Vec2 hitNormal;
        
        for (size_t i = 0; i < sector.walls.size(); i++) {
            const Wall& wall = sector.walls[i];
            Vec2 wallStart = wall.segment.start.position;
            Vec2 wallEnd = wall.segment.end.position;
            
            // Vectors for intersection calculation
            Vec2 v1 = origin - wallStart;
            Vec2 v2 = wallEnd - wallStart;
            Vec2 v3(direction.y, -direction.x);  // Perpendicular to ray direction
            
            // Check if ray and wall are parallel
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
                
                // Calculate surface normal
                Vec2 wallDir = (wallEnd - wallStart).normalized();
                hitNormal = Vec2(-wallDir.y, wallDir.x);
                
                // Make sure normal points back toward ray origin
                if (hitNormal.dotProduct(direction) > 0.0f) {
                    hitNormal = hitNormal * -1.0f;
                }
            }
        }
        
        if (hit && (closestDist < collision.distance)) {
            // Update collision information 
            collision.collision = true;
            collision.distance = closestDist;
            collision.point = hitPoint;
            collision.wallIndex = hitWallIndex;
            collision.sectorId = node->sectorId;
            collision.normal = hitNormal;
        }
        
        return;
    }
    
    // Otherwise, traverse the BSP tree
    Vec2 partDir = node->partitioner.direction();
    Vec2 normal(-partDir.y, partDir.x);  // Normal vector to the partitioner
    
    Vec2 toPartStart = origin - node->partitioner.start.position;
    float side = normal.dotProduct(toPartStart);
    float dirSide = normal.dotProduct(direction);
    
    // Improved traversal logic to ensure proper wall detection
    if (std::abs(side) < 0.0001f) {
        // Ray origin is on the partitioner line - check both sides
        // Start with the side the ray is pointing to
        if (dirSide > 0.0f) {
            if (node->back) castRayRecursive(node->back.get(), origin, direction, maxDistance, collision);
            if (node->front) castRayRecursive(node->front.get(), origin, direction, maxDistance, collision);
        } else {
            if (node->front) castRayRecursive(node->front.get(), origin, direction, maxDistance, collision);
            if (node->back) castRayRecursive(node->back.get(), origin, direction, maxDistance, collision);
        }
    }
    else if (side >= 0.0f) {
        // Origin is in front of the partitioner
        
        // Check front side first
        if (node->front) {
            castRayRecursive(node->front.get(), origin, direction, maxDistance, collision);
        }
        
        // Check if we need to check the back side
        if (dirSide < 0.0f && node->back) {
            // Ray is pointing to back side
            // Calculate intersection with partitioner
            float t = -side / dirSide;
            
            if (t > 0.0f && t < collision.distance) {
                // Ray intersects partitioner before any detected collision
                Vec2 intersectionPoint = origin + direction * t;
                
                // Check if intersection point is on the partitioner segment
                if (node->partitioner.containsPoint(intersectionPoint)) {
                    // Continue from intersection point
                    castRayRecursive(node->back.get(), intersectionPoint, direction, 
                                     collision.distance - t, collision);
                }
            }
        }
    } 
    else {
        // Origin is behind the partitioner
        
        // Check back side first
        if (node->back) {
            castRayRecursive(node->back.get(), origin, direction, maxDistance, collision);
        }
        
        // Check if we need to check the front side
        if (dirSide > 0.0f && node->front) {
            // Ray is pointing to front side
            // Calculate intersection with partitioner
            float t = -side / dirSide;
            
            if (t > 0.0f && t < collision.distance) {
                // Ray intersects partitioner before any detected collision
                Vec2 intersectionPoint = origin + direction * t;
                
                // Check if intersection point is on the partitioner segment
                if (node->partitioner.containsPoint(intersectionPoint)) {
                    // Continue from intersection point
                    castRayRecursive(node->front.get(), intersectionPoint, direction, 
                                    collision.distance - t, collision);
                }
            }
        }
    }
}

// Collision detection for moving objects
CollisionInfo BSPTree::checkCollision(const Vec2& position, float radius, const Vec2& velocity) const {
    CollisionInfo collision;
    
    if (!m_root) {
        return collision;
    }
    
    // Initialize collision info
    collision.collision = false;
    collision.distance = 1.0f; // Normalized distance
    
    // Check collision recursively through the BSP tree
    checkCollisionRecursive(m_root.get(), position, radius, velocity, collision);
    
    return collision;
}

// Recursive function to check for collisions
void BSPTree::checkCollisionRecursive(const BSPNode* node, const Vec2& position, float radius, 
                                    const Vec2& velocity, CollisionInfo& collision) const {
    if (!node) {
        return;
    }
    
    // If this is a leaf node, check for collision with all walls
    if (node->isLeaf) {
        for (size_t i = 0; i < node->walls.size(); ++i) {
            const Wall& wall = node->walls[i];
            
            // Skip non-solid walls
            if (!wall.isSolid) {
                continue;
            }
            
            // Calculate distance from position to wall
            float dist = wall.segment.distanceToPoint(position);
            
            // If position is already too close to the wall - apply a small buffer
            // Using a larger buffer (0.005f) to prevent getting stuck
            if (dist < radius - 0.005f) {
                // Immediate collision
                collision.collision = true;
                collision.distance = 0.0f;
                collision.wallIndex = static_cast<int>(i);
                collision.sectorId = node->sectorId;
                
                // Find closest point on wall
                Vec2 wallDir = (wall.segment.end.position - wall.segment.start.position).normalized();
                Vec2 toWall = position - wall.segment.start.position;
                float proj = toWall.dotProduct(wallDir);
                Vec2 closestPoint = wall.segment.start.position + wallDir * std::max(0.0f, std::min(proj, wall.segment.length()));
                
                // Set collision point and normal
                collision.point = closestPoint;
                
                // Ensure the normal points away from the wall (toward the player)
                Vec2 normal = (position - closestPoint);
                // Make sure we don't get a zero normal
                if (normal.lengthSquared() < 0.0001f) {
                    // Create a perpendicular vector to the wall if we're exactly on the wall
                    normal = Vec2(-wallDir.y, wallDir.x);
                }
                collision.normal = normal.normalized();
                return;
            }
            
            // Check if movement will cause collision with the wall
            if (velocity.lengthSquared() > 0.0001f) {
                // Cast a ray from position in direction of velocity
                Vec2 dir = velocity.normalized();
                float len = velocity.length();
                
                // Create enlarged wall to account for radius
                Vec2 wallDir = (wall.segment.end.position - wall.segment.start.position).normalized();
                Vec2 wallNormal = Vec2(-wallDir.y, wallDir.x);
                
                // Significantly reduce the collision radius to help with narrow passages
                // This gives the player a lot more room to move through tight spaces
                float adjustedRadius = radius * 0.85f; // Reduce to 85% to allow more movement in tight spaces
                
                Vec2 offset = wallNormal * adjustedRadius;
                Line enlargedWall(
                    wall.segment.start.position + offset,
                    wall.segment.end.position + offset
                );
                
                // Check for intersection
                Vec2 v1 = position - enlargedWall.start.position;
                Vec2 v2 = enlargedWall.end.position - enlargedWall.start.position;
                Vec2 v3(-dir.y, dir.x);
                
                float dot = v2.dotProduct(v3);
                // Increased precision threshold further
                if (std::abs(dot) > 0.000001f) {
                    float t1 = v2.crossProduct(v1) / dot;
                    float t2 = v1.dotProduct(v3) / dot;
                    
                    // If we will hit the wall during movement
                    if (t1 >= 0.0f && t1 < len && t2 >= 0.0f && t2 <= 1.0f) {
                        float normalizedDist = t1 / len;
                        if (normalizedDist < collision.distance) {
                            // Record this collision
                            collision.collision = true;
                            collision.distance = normalizedDist;
                            collision.point = position + dir * t1;
                            collision.wallIndex = static_cast<int>(i);
                            collision.sectorId = node->sectorId;
                            collision.normal = wallNormal;
                            
                            // Make sure normal points against movement
                            if (collision.normal.dotProduct(dir) > 0) {
                                collision.normal = collision.normal * -1.0f;
                            }
                        }
                    }
                }
            }
        }
        
        return;
    }
    
    // Otherwise, traverse the BSP tree to the appropriate nodes
    Vec2 partDir = node->partitioner.direction();
    Vec2 normal(-partDir.y, partDir.x);  // Normal vector to the partitioner
    
    Vec2 toPartStart = position - node->partitioner.start.position;
    float side = normal.dotProduct(toPartStart);
    
    // Determine which side(s) to check with more padding
    bool checkFront = (side + radius * 1.1f >= 0.0f); // Add 10% extra padding
    bool checkBack = (side - radius * 1.1f <= 0.0f);  // Add 10% extra padding
    
    // Check both sides if we're near the partition plane - with much wider range
    if (std::abs(side) < radius * 1.5f) {
        // We're close to the partition, check both sides with generous margin
        // to prevent getting stuck at partition boundaries
        checkFront = checkBack = true;
    }
    
    // Check appropriate sides
    if (checkFront && node->front) {
        checkCollisionRecursive(node->front.get(), position, radius, velocity, collision);
    }
    
    if (checkBack && node->back && (!collision.collision || collision.distance > 0.0f)) {
        checkCollisionRecursive(node->back.get(), position, radius, velocity, collision);
    }
}

// Check for convexity in a set of walls
bool BSPTree::isConvex(const std::vector<Wall>& walls) const {
    if (walls.size() <= 3) return true; // Triangles and simpler shapes are always convex
    
    // For a shape to be convex, all interior angles must be less than 180 degrees
    // In a properly ordered wall list, that means all cross products point in the same direction
    
    bool crossProductSign = false; // The sign of the first non-zero cross product
    bool signInitialized = false;
    
    for (size_t i = 0; i < walls.size(); i++) {
        size_t j = (i + 1) % walls.size();
        
        // Get directions of consecutive walls
        Vec2 dir1 = (walls[i].segment.end.position - walls[i].segment.start.position).normalized();
        Vec2 dir2 = (walls[j].segment.end.position - walls[j].segment.start.position).normalized();
        
        // Calculate cross product
        float cross = dir1.crossProduct(dir2);
        
        // Skip pairs of walls that are nearly parallel
        if (std::abs(cross) < 0.0001f) {
            continue;
        }
        
        // Initialize sign with first significant cross product
        if (!signInitialized) {
            crossProductSign = (cross > 0);
            signInitialized = true;
        } 
        // If sign changes, the shape is not convex
        else if ((cross > 0) != crossProductSign) {
            return false;
        }
    }
    
    return true;
}

// Optimize a set of walls by merging colinear segments
std::vector<Wall> BSPTree::optimizeWalls(const std::vector<Wall>& walls) const {
    if (walls.size() <= 1) return walls;
    
    std::vector<Wall> optimized;
    
    for (size_t i = 0; i < walls.size(); i++) {
        const Wall& current = walls[i];
        
        // Skip processing if the wall was already merged
        if (current.segment.start.position.approxEquals(current.segment.end.position)) {
            continue;
        }
        
        // Look for a wall that can be merged with the current one
        bool merged = false;
        for (Wall& existing : optimized) {
            // Check if they share an endpoint and are in the same sector
            bool shareEndpoint = 
                current.segment.start.position.approxEquals(existing.segment.end.position) ||
                current.segment.end.position.approxEquals(existing.segment.start.position);
                
            bool sameSectors = 
                current.sectorFront == existing.sectorFront && 
                current.sectorBack == existing.sectorBack;
                
            if (shareEndpoint && sameSectors) {
                // Calculate directions
                Vec2 dir1 = (existing.segment.end.position - existing.segment.start.position).normalized();
                Vec2 dir2 = (current.segment.end.position - current.segment.start.position).normalized();
                
                // Check if they are nearly parallel
                float dot = dir1.dotProduct(dir2);
                if (std::abs(dot - 1.0f) < 0.0001f || std::abs(dot + 1.0f) < 0.0001f) {
                    // Merge by extending the existing wall
                    if (current.segment.start.position.approxEquals(existing.segment.end.position)) {
                        existing.segment.end = current.segment.end;
                    } else {
                        existing.segment.start = current.segment.start;
                    }
                    merged = true;
                    break;
                }
            }
        }
        
        // If not merged, add as a new wall
        if (!merged) {
            optimized.push_back(current);
        }
    }
    
    return optimized;
}

// Update moving sectors and platforms
void BSPTree::update(float deltaTime) {
    // Update all moving sectors
    bool anySectorMoved = false;
    
    for (Sector& sector : m_sectors) {
        if (sector.isMoving() && sector.movementActive) {
            sector.update(deltaTime);
            anySectorMoved = true;
        }
    }
    
    // Update moving platforms
    bool anyPlatformMoved = false;
    for (Platform& platform : m_platforms) {
        if (platform.isMoving) {
            platform.update(deltaTime);
            anyPlatformMoved = true;
        }
    }
    
    // If any sector moved, we need to rebuild the BSP tree
    if (anySectorMoved) {
        // Rebuild the BSP tree with the updated sectors
        // Collect all walls from all sectors
        std::vector<Wall> allWalls;
        try {
            for (size_t i = 0; i < m_sectors.size(); ++i) {
                const Sector& sector = m_sectors[i];
                
                for (const Wall& wall : sector.walls) {
                    Wall wallCopy = wall;
                    if (wallCopy.sectorFront == -1) {
                        wallCopy.sectorFront = static_cast<int>(i);
                    }
                    allWalls.push_back(wallCopy);
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "ERROR: Exception during wall collection: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "ERROR: Unknown exception during wall collection!" << std::endl;
        }
        
        // Rebuild the tree
        try {
            m_root = buildTree(std::move(allWalls));
        }
        catch (const std::exception& e) {
            std::cerr << "Exception during BSP rebuild: " << e.what() << std::endl;
            // Continue with the old BSP tree rather than crashing
        }
        catch (...) {
            std::cerr << "Unknown exception during BSP rebuild!" << std::endl;
            // Continue with the old BSP tree rather than crashing
        }
    }
}

// Trigger a sector by tag
void BSPTree::triggerSector(const std::string& tag) {
    auto it = m_tagToSectors.find(tag);
    if (it != m_tagToSectors.end()) {
        for (int sectorId : it->second) {
            if (sectorId >= 0 && sectorId < static_cast<int>(m_sectors.size())) {
                m_sectors[sectorId].trigger();
            }
        }
    }
}

// Check if a sector is visible from a viewpoint
bool BSPTree::isSectorVisible(int sectorId, const Vec2& viewPosition, float viewAngle, float fov) const {
    if (sectorId < 0 || sectorId >= static_cast<int>(m_sectors.size())) {
        return false;
    }
    
    // The viewer's sector is always visible
    int viewerSector = findSector(viewPosition);
    if (viewerSector == sectorId) {
        return true;
    }
    
    // Find portals connecting to this sector
    for (const Sector& sector : m_sectors) {
        for (const Wall& wall : sector.walls) {
            if (wall.isPortal() && 
                (wall.sectorBack == sectorId || wall.sectorFront == sectorId)) {
                // Found a portal to the target sector, check if it's visible
                if (isPortalVisible(wall, viewPosition, viewAngle, fov)) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

// Check if a portal is potentially visible from a viewpoint
bool BSPTree::isPortalVisible(const Wall& portalWall, const Vec2& viewPosition, 
                            float viewAngle, float fov) const {
    // Calculate angle to each portal endpoint
    Vec2 toStart = portalWall.segment.start.position - viewPosition;
    Vec2 toEnd = portalWall.segment.end.position - viewPosition;
    
    float angleStart = std::atan2(toStart.y, toStart.x);
    float angleEnd = std::atan2(toEnd.y, toEnd.x);
    
    // Make sure the angle difference is correct (handles wrap-around)
    float angleDiff = angleEnd - angleStart;
    if (angleDiff > 3.14159f) angleDiff -= 6.28318f;
    if (angleDiff < -3.14159f) angleDiff += 6.28318f;
    
    // Calculate the view cone
    float halfFov = fov * 0.5f * 0.01745329f; // Convert to radians
    float minViewAngle = viewAngle - halfFov;
    float maxViewAngle = viewAngle + halfFov;
    
    // Normalize all angles to [0, 2π)
    while (minViewAngle < 0) minViewAngle += 6.28318f;
    while (maxViewAngle < 0) maxViewAngle += 6.28318f;
    while (angleStart < 0) angleStart += 6.28318f;
    while (angleEnd < 0) angleEnd += 6.28318f;
    
    // Check if the portal overlaps with the view cone
    if (minViewAngle < maxViewAngle) {
        // Normal case
        if ((angleStart >= minViewAngle && angleStart <= maxViewAngle) ||
            (angleEnd >= minViewAngle && angleEnd <= maxViewAngle) ||
            (angleStart <= minViewAngle && angleEnd >= maxViewAngle)) {
            return true;
        }
    } else {
        // View cone wraps around
        if ((angleStart >= minViewAngle || angleStart <= maxViewAngle) ||
            (angleEnd >= minViewAngle || angleEnd <= maxViewAngle) ||
            (angleStart <= minViewAngle && angleEnd >= maxViewAngle)) {
            return true;
        }
    }
    
    return false;
}

// Find visible portals from a viewpoint
std::vector<int> BSPTree::findVisiblePortals(int startSectorId, const Vec2& viewPosition, 
                                           float viewAngle, float fov) const {
    // Use a helper function with recursion depth limit to prevent stack overflow
    std::unordered_map<int, bool> processedSectors;
    return findVisiblePortalsRecursive(startSectorId, viewPosition, viewAngle, fov, 
                                      processedSectors, 0, 10); // Max recursion depth of 10
}

// Helper function with recursion depth tracking
std::vector<int> BSPTree::findVisiblePortalsRecursive(int currentSectorId, const Vec2& viewPosition,
                                                   float viewAngle, float fov,
                                                   std::unordered_map<int, bool>& processedSectors,
                                                   int currentDepth, int maxDepth) const {
    std::vector<int> visibleSectors;
    
    // Check for invalid sector ID or excessive recursion depth
    if (currentSectorId < 0 || 
        currentSectorId >= static_cast<int>(m_sectors.size()) ||
        currentDepth >= maxDepth ||
        processedSectors.find(currentSectorId) != processedSectors.end()) {
        return visibleSectors;
    }
    
    // Mark this sector as processed to prevent cycles
    processedSectors[currentSectorId] = true;
    
    // Add the current sector
    visibleSectors.push_back(currentSectorId);
    
    // Check all portal walls in the current sector
    const Sector& sector = m_sectors[currentSectorId];
    for (const Wall& wall : sector.walls) {
        // Skip non-portal walls
        if (!wall.isPortal()) {
            continue;
        }
        
        int nextSector = (wall.sectorFront == currentSectorId) ? wall.sectorBack : wall.sectorFront;
        
        // Skip invalid sectors or already processed sectors
        if (nextSector < 0 || 
            nextSector >= static_cast<int>(m_sectors.size()) ||
            processedSectors.find(nextSector) != processedSectors.end()) {
            continue;
        }
        
        // Check if the portal is visible
        if (isPortalVisible(wall, viewPosition, viewAngle, fov)) {
            // Calculate the center point of the portal wall for the next viewpoint
            Vec2 portalCenter = (wall.segment.start.position + wall.segment.end.position) * 0.5f;
            
            // Recursively find visible sectors through this portal
            std::vector<int> nextVisible = findVisiblePortalsRecursive(
                nextSector, portalCenter, viewAngle, fov, 
                processedSectors, currentDepth + 1, maxDepth
            );
            
            // Add all newly found visible sectors
            for (int secId : nextVisible) {
                if (std::find(visibleSectors.begin(), visibleSectors.end(), secId) == visibleSectors.end()) {
                    visibleSectors.push_back(secId);
                }
            }
        }
    }
    
    return visibleSectors;
}

// Validate the BSP tree
bool BSPTree::validate() const {
    if (!m_root) {
        std::cout << "BSP Tree is empty - nothing to validate." << std::endl;
        return true;
    }
    
    bool valid = validateRecursive(m_root.get());
    
    if (valid) {
        std::cout << "BSP Tree validation successful." << std::endl;
    } else {
        std::cout << "BSP Tree validation failed." << std::endl;
    }
    
    return valid;
}

// Recursive function to validate the BSP tree
bool BSPTree::validateRecursive(const BSPNode* node) const {
    if (!node) {
        return true;
    }
    
    // If this is a leaf node, check that it has a valid sector ID
    if (node->isLeaf) {
        if (node->sectorId < 0 || node->sectorId >= static_cast<int>(m_sectors.size())) {
            std::cerr << "Invalid sector ID: " << node->sectorId << std::endl;
            return false;
        }
        return true;
    }
    
    // Check front subtree
    bool frontValid = validateRecursive(node->front.get());
    
    // Check back subtree
    bool backValid = validateRecursive(node->back.get());
    
    return frontValid && backValid;
}

// Add a platform to the scene
void BSPTree::addPlatform(const Platform& platform) {
    // Add the platform to the list
    m_platforms.push_back(platform);
    
    // Update the tag-to-platform map
    int platformIndex = static_cast<int>(m_platforms.size() - 1);
    if (!platform.tag.empty()) {
        m_tagToPlatforms[platform.tag].push_back(platformIndex);
    }
    
    // Ensure the platform has a valid sector ID
    if (platform.sectorId < 0 || platform.sectorId >= static_cast<int>(m_sectors.size())) {
        std::cerr << "Warning: Platform has invalid sector ID: " << platform.sectorId << std::endl;
    }
}

// Check if a point is on a platform
bool BSPTree::isPointOnPlatform(const Vec2& point, float height, int& platformIndex) const {
    platformIndex = -1;
    
    // Check each platform
    for (size_t i = 0; i < m_platforms.size(); ++i) {
        const Platform& platform = m_platforms[i];
        
        // Check if the point is within the platform's 2D bounds
        if (platform.containsPoint(point)) {
            // Check if the height is within the platform's height range
            float topHeight = platform.getTopHeight();
            float bottomHeight = platform.getBottomHeight();
            
            // Use a more lenient height check with a small buffer (0.1 units)
            const float HEIGHT_BUFFER = 0.1f;
            if (height >= bottomHeight - HEIGHT_BUFFER && height <= topHeight + HEIGHT_BUFFER) {
                platformIndex = static_cast<int>(i);
                
                // Debug output
                std::cout << "Platform detected at (" << point.x << ", " << point.y 
                          << "), type: " << (platform.type == PlatformType::STAIR ? "STAIR" : "OTHER")
                          << ", index: " << i
                          << ", height range: " << bottomHeight << " to " << topHeight
                          << ", player height: " << height << std::endl;
                
                return true;
            }
            
            // Debug output for near misses
            if (std::abs(height - topHeight) < 0.2f || std::abs(height - bottomHeight) < 0.2f) {
                std::cout << "Near miss platform at (" << point.x << ", " << point.y 
                          << "), height range: " << bottomHeight << " to " << topHeight
                          << ", player height: " << height << std::endl;
            }
        }
    }
    
    return false;
}

// Check if a ray intersects with a platform
bool BSPTree::rayIntersectsPlatform(const Vec2& origin, const Vec2& direction, float maxDistance,
                                  Vec2& hitPoint, float& hitHeight, int& platformIndex) const {
    platformIndex = -1;
    float closestDistance = maxDistance;
    
    // Check each platform
    for (size_t i = 0; i < m_platforms.size(); ++i) {
        const Platform& platform = m_platforms[i];
        
        // Check if the ray intersects with the platform's 2D bounds
        // This is a simplified approach - we're casting a ray and checking if it hits any of the platform's edges
        
        // For each edge of the platform
        for (size_t j = 0; j < platform.vertices.size(); ++j) {
            size_t nextIndex = (j + 1) % platform.vertices.size();
            
            // Create a line segment for this edge
            Line edge(Vertex(platform.vertices[j]), Vertex(platform.vertices[nextIndex]));
            
            // Check if the ray intersects with this edge
            Vec2 intersection;
            float t1, t2;
            
            // Ray equation: origin + t1 * direction
            // Edge equation: edge.start.position + t2 * (edge.end.position - edge.start.position)
            
            // Solve for t1 and t2
            Vec2 edgeDir = edge.end.position - edge.start.position;
            float crossProduct = direction.crossProduct(edgeDir);
            
            // If crossProduct is zero, lines are parallel
            if (std::abs(crossProduct) < 0.0001f) {
                continue;
            }
            
            Vec2 originToStart = edge.start.position - origin;
            t1 = originToStart.crossProduct(edgeDir) / crossProduct;
            t2 = originToStart.crossProduct(direction) / crossProduct;
            
            // Check if intersection is valid
            if (t1 >= 0.0f && t1 <= closestDistance && t2 >= 0.0f && t2 <= 1.0f) {
                // Calculate intersection point
                intersection = origin + direction * t1;
                
                // Check if this is the closest intersection
                if (t1 < closestDistance) {
                    closestDistance = t1;
                    hitPoint = intersection;
                    
                    // Calculate the height at the intersection point
                    // For a flat platform, the height is constant
                    hitHeight = platform.getTopHeight();
                    platformIndex = static_cast<int>(i);
                }
            }
        }
    }
    
    return platformIndex != -1;
}

} // namespace PureDoom 