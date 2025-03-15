#include "Renderer.h"
#include "TextureLoader.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <chrono>

namespace PureDoom {

// Color utilities
Color Color::fromHSV(float h, float s, float v) {
    if (s <= 0.0f) return Color(static_cast<uint8_t>(v * 255), 
                               static_cast<uint8_t>(v * 255), 
                               static_cast<uint8_t>(v * 255));

    h = std::fmod(h, 360.0f) / 60.0f;
    int i = static_cast<int>(h);
    float f = h - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    float r, g, b;
    switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }

    return Color(static_cast<uint8_t>(r * 255), 
                static_cast<uint8_t>(g * 255), 
                static_cast<uint8_t>(b * 255));
}

Color Color::blend(const Color& c1, const Color& c2, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    float invT = 1.0f - t;
    
    return Color(
        static_cast<uint8_t>(c1.r * invT + c2.r * t),
        static_cast<uint8_t>(c1.g * invT + c2.g * t),
        static_cast<uint8_t>(c1.b * invT + c2.b * t),
        static_cast<uint8_t>(c1.a * invT + c2.a * t)
    );
}

// Texture implementation
Texture::Texture(int width, int height) : m_width(width), m_height(height) {
    m_pixels.resize(width * height, Color(0, 0, 0, 255));
    generateCheckerboard();
}

Texture::Texture(const std::string& filename) : m_width(0), m_height(0) {
    loadFromFile(filename);
}

Color Texture::getPixel(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return Color(255, 0, 255); // Magenta for out of bounds
    }
    return m_pixels[y * m_width + x];
}

Color Texture::sample(float u, float v) const {
    // Wrap texture coordinates
    u = u - std::floor(u);
    v = v - std::floor(v);
    
    // Convert to pixel coordinates
    int x = static_cast<int>(u * m_width);
    int y = static_cast<int>(v * m_height);
    
    // Clamp to texture size
    x = std::max(0, std::min(m_width - 1, x));
    y = std::max(0, std::min(m_height - 1, y));
    
    return getPixel(x, y);
}

void Texture::loadFromFile(const std::string& filename) {
    // Try to load the texture using TextureLoader
    std::shared_ptr<Texture> loadedTex = TextureLoader::loadTexture(filename);
    
    if (loadedTex) {
        // Copy the loaded texture data
        m_width = loadedTex->m_width;
        m_height = loadedTex->m_height;
        m_pixels = loadedTex->m_pixels;
        std::cout << "Loaded texture: " << filename << " (" << m_width << "x" << m_height << ")" << std::endl;
    } else {
        // Fallback to a procedural pattern if loading fails
        m_width = 64;
        m_height = 64;
        m_pixels.resize(m_width * m_height);
        generateCheckerboard();
        std::cout << "Failed to load texture file: " << filename << ", generated fallback pattern instead" << std::endl;
    }
}

void Texture::generateCheckerboard() {
    const int tileSize = 8;
    for (int y = 0; y < m_height; y++) {
        for (int x = 0; x < m_width; x++) {
            bool isEvenTileX = ((x / tileSize) % 2) == 0;
            bool isEvenTileY = ((y / tileSize) % 2) == 0;
            bool isLight = isEvenTileX != isEvenTileY;
            
            Color color = isLight ? Color(200, 200, 200) : Color(70, 70, 70);
            
            // Add a border
            if (x % tileSize == 0 || y % tileSize == 0) {
                color = Color(120, 120, 120);
            }
            
            m_pixels[y * m_width + x] = color;
        }
    }
}

// Renderer implementation
Renderer::Renderer(int width, int height) : m_width(width), m_height(height) {
    m_frameBuffer.resize(width * height);
    m_zBuffer.resize(width * height, std::numeric_limits<float>::max());
    m_wallExtents.resize(width);
}

Renderer::~Renderer() {
    // Clean up TextureLoader
    TextureLoader::shutdown();
}

void Renderer::initialize() {
    // Initialize texture loader
    if (!TextureLoader::initialize()) {
        std::cerr << "Failed to initialize texture loader" << std::endl;
    }
    
    loadTextures();
    loadSpriteTextures();
    clearBuffers();
}

void Renderer::loadTextures() {
    // First ensure we have a default texture
    Texture defaultTexture(64, 64); // Default checkerboard
    m_textures.push_back(defaultTexture);
    
    // Now use the TextureLoader to create some procedural textures
    
    // Wall textures
    for (int i = 0; i < 10; i++) {
        std::shared_ptr<Texture> tex;
        
        if (i % 3 == 0) {
            tex = TextureLoader::createProceduralTexture(64, 64, "brick");
        } else if (i % 3 == 1) {
            tex = TextureLoader::createProceduralTexture(64, 64, "checkerboard");
        } else {
            tex = TextureLoader::createProceduralTexture(64, 64, "gradient");
        }
        
        if (tex) {
            m_textures.push_back(*tex);
        }
    }
    
    // Try to load some textures from files
    std::vector<std::string> textureFiles = {
        "textures/wall1.png", 
        "textures/wall2.png", 
        "textures/floor1.png", 
        "textures/ceiling1.png"
    };
    
    for (const auto& file : textureFiles) {
        std::shared_ptr<Texture> tex = TextureLoader::loadTexture(file);
        if (tex) {
            m_textures.push_back(*tex);
            std::cout << "Loaded texture: " << file << std::endl;
        } else {
            // If file loading failed, add a procedural texture instead
            tex = TextureLoader::createProceduralTexture(64, 64, "checkerboard");
            if (tex) {
                m_textures.push_back(*tex);
            }
        }
    }
    
    std::cout << "Loaded " << m_textures.size() << " textures" << std::endl;
}

void Renderer::loadSpriteTextures() {
    // List current textures
    std::cout << "Before loading sprite textures, we have " << m_textures.size() << " textures" << std::endl;

    // Try to load sprite textures from files first
    std::vector<std::string> spriteTextureFiles = {
        "textures/sprite_blue.png",
        "textures/sprite_green.png",
        "textures/sprite_red.png"
    };
    
    int blueTextureIndex = -1;
    int greenTextureIndex = -1;
    int redTextureIndex = -1;
    
    // Try to load from files
    for (size_t i = 0; i < spriteTextureFiles.size(); i++) {
        std::shared_ptr<Texture> tex = TextureLoader::loadTexture(spriteTextureFiles[i]);
        if (tex) {
            m_textures.push_back(*tex);
            
            // Store texture indices
            if (i == 0) blueTextureIndex = m_textures.size() - 1;
            else if (i == 1) greenTextureIndex = m_textures.size() - 1;
            else if (i == 2) redTextureIndex = m_textures.size() - 1;
            
            std::cout << "Loaded sprite texture: " << spriteTextureFiles[i] << std::endl;
        }
    }
    
    // If any textures failed to load, create procedural textures
    if (blueTextureIndex == -1) {
        // Create a simple sprite texture with a filled circle
        Texture spriteTexture(64, 64);
        int width = spriteTexture.width();
        int height = spriteTexture.height();
        
        // Add a colored circle sprite texture
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float centerX = width / 2.0f;
                float centerY = height / 2.0f;
                float dx = x - centerX;
                float dy = y - centerY;
                float distance = std::sqrt(dx * dx + dy * dy);
                
                if (distance < width / 2.0f) {
                    // Inside circle
                    float t = distance / (width / 2.0f);
                    Color color = Color::fromHSV(240.0f, 0.7f, 1.0f - t * 0.5f);
                    spriteTexture.m_pixels[y * width + x] = color;
                } else {
                    // Outside circle (transparent)
                    spriteTexture.m_pixels[y * width + x] = Color(0, 0, 0, 0);
                }
            }
        }
        
        m_textures.push_back(spriteTexture);
        blueTextureIndex = m_textures.size() - 1;
    }
    
    if (greenTextureIndex == -1) {
        // Add a simple item sprite (gem)
        Texture itemTexture(64, 64);
        int width = itemTexture.width();
        int height = itemTexture.height();
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float centerX = width / 2.0f;
                float centerY = height / 2.0f;
                float dx = x - centerX;
                float dy = y - centerY;
                
                // Diamond shape
                float distance = std::abs(dx) + std::abs(dy);
                
                if (distance < width / 2.0f) {
                    // Inside diamond
                    float t = distance / (width / 2.0f);
                    Color color = Color::fromHSV(120.0f, 0.8f, 1.0f - t * 0.3f);
                    itemTexture.m_pixels[y * width + x] = color;
                } else {
                    // Outside diamond (transparent)
                    itemTexture.m_pixels[y * width + x] = Color(0, 0, 0, 0);
                }
            }
        }
        
        m_textures.push_back(itemTexture);
        greenTextureIndex = m_textures.size() - 1;
    }
    
    if (redTextureIndex == -1) {
        // Create a simple enemy sprite (red diamond)
        Texture enemyTexture(64, 64);
        int width = enemyTexture.width();
        int height = enemyTexture.height();
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float centerX = width / 2.0f;
                float centerY = height / 2.0f;
                float dx = x - centerX;
                float dy = y - centerY;
                
                // Diamond shape
                float distance = std::abs(dx) + std::abs(dy);
                
                if (distance < width / 2.0f) {
                    // Inside diamond
                    float t = distance / (width / 2.0f);
                    Color color = Color::fromHSV(0.0f, 0.9f, 1.0f - t * 0.3f);
                    enemyTexture.m_pixels[y * width + x] = color;
                } else {
                    // Outside diamond (transparent)
                    enemyTexture.m_pixels[y * width + x] = Color(0, 0, 0, 0);
                }
            }
        }
        
        m_textures.push_back(enemyTexture);
        redTextureIndex = m_textures.size() - 1;
    }
    
    std::cout << "Loaded " << m_textures.size() << " textures (including sprite textures)" << std::endl;
    std::cout << "Blue sprite texture index: " << blueTextureIndex << std::endl;
    std::cout << "Green sprite texture index: " << greenTextureIndex << std::endl;
    std::cout << "Red sprite texture index: " << redTextureIndex << std::endl;
    std::cout << "Sprites in main.cpp should use these texture IDs!" << std::endl;
}

void Renderer::clearBuffers() {
    // Clear the frame buffer to black
    std::fill(m_frameBuffer.begin(), m_frameBuffer.end(), Color(0, 0, 0));
    
    // Reset the z-buffer
    std::fill(m_zBuffer.begin(), m_zBuffer.end(), std::numeric_limits<float>::max());
}

void Renderer::renderFrame(const BSPTree& bsp, const ViewPosition& view, const std::vector<Sprite>& sprites) {
    clearBuffers();
    
    // Reset wall extents for this frame
    for (auto& extent : m_wallExtents) {
        extent.top = m_height;
        extent.bottom = 0;
    }
    
    // Clear visplanes
    m_visplanes.clear();
    
    // Calculate delta time for this frame (using a fixed value for now)
    // In a real implementation, you would pass this as a parameter
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;
    
    // Render the skybox first (background)
    renderSkybox(view, deltaTime);
    
    // Render the BSP tree (walls)
    renderBSP(bsp, view);
    
    // Render floors and ceilings using span-based approach
    renderFloorAndCeilingSpans(bsp, view);
    
    // Render sprites
    renderSprites(bsp, view, sprites);
}

void Renderer::renderBSP(const BSPTree& bsp, const ViewPosition& view) {
    // DOOM-style column-based rendering
    float halfFov = view.fov * 0.5f * DEG_TO_RAD;
    float angleStep = view.fov * DEG_TO_RAD / m_width;
    
    // Use the max view distance from skybox for distance culling
    float maxDistance = m_skybox.maxViewDistance;
    
    // Calculate reference horizontal plane at player's feet
    // This ensures walls don't distort when player height changes
    float floorLevel = 0.0f; // Assume ground level is 0
    float ceilingLevel = 0.0f;
    int playerSectorId = bsp.findSector(view.position);
    if (playerSectorId >= 0 && playerSectorId < static_cast<int>(bsp.getSectors().size())) {
        const Sector& playerSector = bsp.getSectors()[playerSectorId];
        floorLevel = playerSector.floorHeight;
        ceilingLevel = playerSector.ceilingHeight;
    }
    
    // Cast a ray for each column of the screen
    for (int x = 0; x < m_width; x++) {
        // Calculate ray angle
        float rayAngle = view.angle - halfFov + angleStep * x;
        
        // Normalize angle to [0, 2π)
        while (rayAngle < 0) rayAngle += 2 * PI;
        while (rayAngle >= 2 * PI) rayAngle -= 2 * PI;
        
        // Create ray direction vector
        Vec2 rayDir(std::cos(rayAngle), std::sin(rayAngle));
        
        // Cast ray and get collision info - use maxDistance for performance
        CollisionInfo collision = bsp.castRay(view.position, rayDir, maxDistance);
        
        if (collision.collision) {
            // Find which sector the wall belongs to
            int sectorId = collision.sectorId;
            if (sectorId >= 0 && sectorId < static_cast<int>(bsp.getSectors().size())) {
                const Sector& sector = bsp.getSectors()[sectorId];
                
                // Create a wall slice for rendering
                WallSlice slice;
                slice.x = x;
                
                // Correct for fisheye effect
                float correctedDistance = collision.distance * std::cos(rayAngle - view.angle);
                slice.distance = correctedDistance;
                
                // Set floor and ceiling heights for proper wall rendering
                slice.floorHeight = sector.floorHeight;
                slice.ceilingHeight = sector.ceilingHeight;
                
                // Calculate perceived wall height with perspective projection
                float wallHeight = sector.ceilingHeight - sector.floorHeight;
                slice.height = calculateWallHeight(correctedDistance, wallHeight);
                
                // Calculate texture coordinate based on hit point and identify portal properties
                Vec2 wallStart, wallEnd;
                bool isProblematicPortal = false;
                float adjacentSectorHeight = 0.0f;
                
                if (collision.wallIndex >= 0 && collision.wallIndex < static_cast<int>(sector.walls.size())) {
                    const Wall& wall = sector.walls[collision.wallIndex];
                    wallStart = wall.segment.start.position;
                    wallEnd = wall.segment.end.position;
                    
                    slice.textureId = wall.textureId >= 0 ? wall.textureId % m_textures.size() : 0;
                    slice.isPortal = wall.isPortal();
                    
                    // Calculate texture U coordinate
                    Vec2 wallVec = wallEnd - wallStart;
                    float wallLength = wallVec.length();
                    Vec2 hitVec = collision.point - wallStart;
                    slice.texCoordU = hitVec.dotProduct(wallVec.normalized()) / wallLength;
                    
                    // Check if this is a portal with special properties that might cause flickering
                    if (slice.isPortal && wall.sectorBack >= 0 && 
                        wall.sectorBack < static_cast<int>(bsp.getSectors().size())) {
                        
                        const Sector& adjacentSector = bsp.getSectors()[wall.sectorBack];
                        adjacentSectorHeight = adjacentSector.ceilingHeight;
                        
                        // Check if the height difference is very small (potential z-fighting cause)
                        float heightDiff = std::abs(sector.ceilingHeight - adjacentSector.ceilingHeight);
                        float floorDiff = std::abs(sector.floorHeight - adjacentSector.floorHeight);
                        
                        // Flag portals with nearly equal heights or narrow portals
                        if (heightDiff < 0.05f || floorDiff < 0.05f || wallLength < 0.5f) {
                            isProblematicPortal = true;
                        }
                    }
                }
                
                // Apply light level from sector
                slice.lightLevel = sector.lightLevel;
                
                // Pass portal detection to the wall slice renderer
                slice.isProblematicPortal = isProblematicPortal;
                slice.adjacentSectorHeight = adjacentSectorHeight;
                
                // Render the wall slice with proper height adjustment
                renderWallSlice(slice, view);
                
                // Calculate wall extents for floor and ceiling rendering
                int centerY = m_height / 2;
                
                // Calculate player's eye height relative to the floor
                float playerHeightAboveFloor = view.height - slice.floorHeight;
                float playerHeightBelowCeiling = slice.ceilingHeight - view.height;
                
                // Calculate the scaling factor for perspective projection
                float scale = DISTANCE_MULTIPLIER / slice.distance;
                
                // Calculate wall top and bottom pixels based on player height
                int wallTop = centerY - static_cast<int>(playerHeightBelowCeiling * scale);
                int wallBottom = centerY + static_cast<int>(playerHeightAboveFloor * scale);
                
                // Clamp to screen bounds
                wallTop = std::max(0, wallTop);
                wallBottom = std::min(m_height - 1, wallBottom);
                
                // Store wall extents for this column
                m_wallExtents[x].top = wallTop;
                m_wallExtents[x].bottom = wallBottom;
                
                // Adjust visplane boundaries at problematic portals to prevent seams
                int floorYStart = wallBottom;
                int ceilingYEnd = wallTop;
                
                // For problematic portals, extend the visplane boundaries slightly
                if (isProblematicPortal) {
                    floorYStart = std::min(m_height - 1, wallBottom + 1);
                    ceilingYEnd = std::max(0, wallTop - 1);
                }
                
                // Add the floor visplane for this sector (will check if already exists)
                if (floorYStart < m_height - 1) {
                    bool found = false;
                    for (auto& plane : m_visplanes) {
                        if (plane.isFloor && std::abs(plane.height - sector.floorHeight) < 0.001f &&
                            plane.textureId == sector.floorTextureId) {
                            // Update existing visplane
                            plane.columns[x].yStart = floorYStart;
                            plane.columns[x].yEnd = m_height - 1;
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found) {
                        // Create a new visplane
                        Visplane floorPlane;
                        floorPlane.isFloor = true;
                        floorPlane.height = sector.floorHeight;
                        floorPlane.textureId = sector.floorTextureId;
                        floorPlane.lightLevel = sector.lightLevel;
                        floorPlane.columns.resize(m_width);
                        floorPlane.columns[x].yStart = floorYStart;
                        floorPlane.columns[x].yEnd = m_height - 1;
                        m_visplanes.push_back(floorPlane);
                    }
                }
                
                // Add the ceiling visplane for this sector
                if (ceilingYEnd > 0) {
                    bool found = false;
                    for (auto& plane : m_visplanes) {
                        if (!plane.isFloor && std::abs(plane.height - sector.ceilingHeight) < 0.001f &&
                            plane.textureId == sector.ceilingTextureId) {
                            // Update existing visplane
                            plane.columns[x].yStart = 0;
                            plane.columns[x].yEnd = ceilingYEnd;
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found) {
                        // Create a new visplane
                        Visplane ceilingPlane;
                        ceilingPlane.isFloor = false;
                        ceilingPlane.height = sector.ceilingHeight;
                        ceilingPlane.textureId = sector.ceilingTextureId;
                        ceilingPlane.lightLevel = sector.lightLevel;
                        ceilingPlane.columns.resize(m_width);
                        ceilingPlane.columns[x].yStart = 0;
                        ceilingPlane.columns[x].yEnd = ceilingYEnd;
                        m_visplanes.push_back(ceilingPlane);
                    }
                }
            }
        }
    }
}

void Renderer::renderWallSlice(const WallSlice& slice, const ViewPosition& view) {
    if (slice.x < 0 || slice.x >= m_width) return;
    
    // Calculate wall top and bottom on screen based on player height relative to floor
    int centerY = m_height / 2;
    
    // Calculate vertical offsets based on player's eye height relative to the floor
    float playerHeightAboveFloor = view.height - slice.floorHeight;
    float playerHeightBelowCeiling = slice.ceilingHeight - view.height;
    
    // Calculate how much of the wall should be below and above the horizontal centerline
    float wallHeight = slice.ceilingHeight - slice.floorHeight;
    float projectedHeight = calculateWallHeight(slice.distance, wallHeight);
    
    // Calculate the scaling factor for perspective projection
    float scale = DISTANCE_MULTIPLIER / slice.distance;
    
    // Determine vertical placement
    int wallTop = centerY - static_cast<int>(playerHeightBelowCeiling * scale);
    int wallBottom = centerY + static_cast<int>(playerHeightAboveFloor * scale);
    
    // Clamp to screen bounds
    int clampedTop = std::max(0, wallTop);
    int clampedBottom = std::min(m_height - 1, wallBottom);
    
    // Get the texture for this wall
    int texIndex = slice.textureId % m_textures.size();
    const Texture& tex = m_textures[texIndex];
    
    // Calculate lighting factor (0-1)
    float lightFactor = std::min(1.0f, std::max(0.0f, slice.lightLevel / 255.0f));
    
    // Calculate inverse Z for perspective correction
    float invZ = 1.0f / std::max(0.1f, slice.distance);
    float uOverZ = slice.texCoordU * invZ;
    
    // Apply a depth bias for z-fighting prevention
    float depthBias = 0.0f;
    
    // Standard portal bias
    if (slice.isPortal) {
        depthBias = 0.01f; // Small bias to push portal walls slightly back
    }
    
    // Enhanced bias for problematic portals
    if (slice.isProblematicPortal) {
        // Use a larger bias for problematic portals
        depthBias = 0.05f;
        
        // Add some jitter prevention - fix to a specific multiple to ensure stability
        float distSnapped = std::floor(slice.distance * 100.0f) / 100.0f;
        
        // For problematic portals, ensure the depth is consistently the same for this column
        // by using the portal ID or texture ID to create stable offsets
        float stableOffset = (slice.textureId * 0.001f) + (slice.x % 2) * 0.0005f; 
        depthBias += stableOffset;
    }
    
    // Draw the wall slice
    for (int y = clampedTop; y <= clampedBottom; y++) {
        // Calculate vertical position within wall (0 = ceiling, 1 = floor)
        float normalizedY = 0.0f;
        if (wallBottom != wallTop) {  // Avoid division by zero
            normalizedY = static_cast<float>(y - wallTop) / (wallBottom - wallTop);
        }
        
        // Special handling for problematic portals to prevent flickering
        float u = slice.texCoordU;
        float v = normalizedY;
        
        if (slice.isProblematicPortal) {
            // Ensure stable texture coordinates by snapping to a grid
            u = std::floor(u * 64.0f) / 64.0f;
            
            // Apply a subtle fixed offset based on the column to avoid uniform patterns
            float columnBias = (slice.x % 4) * 0.005f;
            u += columnBias;
            
            // Ensure u is in the [0,1] range
            u = u - std::floor(u);
        }
        
        // Apply subtle perspective effect based on viewing angle
        float distFromCenter = std::abs(normalizedY - 0.5f) * 2.0f; // 0 at center, 1 at edges
        float depthAdjustment = slice.distance * (1.0f + distFromCenter * 0.02f) + depthBias; // Subtle curve + portal bias
        
        // For problematic portals, use a consistent depth rather than a curved one
        if (slice.isProblematicPortal) {
            depthAdjustment = slice.distance + depthBias;
        }
        
        Color texColor = tex.sample(u, v);
        
        // Apply lighting
        Color finalColor = Color::blend(Color(0, 0, 0), texColor, lightFactor);
        
        // Apply distance fog
        float fogFactor = 1.0f - std::min(1.0f, slice.distance / 30.0f);
        finalColor = Color::blend(Color(0, 0, 0), finalColor, fogFactor);
        
        // Draw the pixel with depth information
        drawPixelWithDepth(slice.x, y, depthAdjustment, finalColor);
    }
}

void Renderer::renderFloorAndCeilingSpans(const BSPTree& bsp, const ViewPosition& view) {
    // Process each visplane and render it
    for (const auto& visplane : m_visplanes) {
        renderVisplane(visplane, view);
    }
}

void Renderer::renderVisplane(const Visplane& visplane, const ViewPosition& view) {
    // Scan for valid y-coordinates
    int minY = m_height;
    int maxY = 0;
    
    // Find the min and max valid rows for this visplane
    for (int x = 0; x < m_width; x++) {
        const auto& column = visplane.columns[x];
        if (column.yStart >= 0 && column.yEnd >= 0) {
            minY = std::min(minY, column.yStart);
            maxY = std::max(maxY, column.yEnd);
        }
    }
    
    // Skip if no valid rows
    if (minY > maxY) return;
    
    // Use more stable horizon calculation for portal areas
    int centerY = m_height / 2;
    float horizonY = static_cast<float>(centerY);
    
    // Render each scanline
    for (int y = minY; y <= maxY; y++) {
        // Skip if y is out of screen bounds
        if (y < 0 || y >= m_height) continue;
        
        // Calculate the world z at this scanline with special handling around the horizon
        float yOffset;
        
        // Handle pixels close to the horizon specially to prevent flickering
        if (std::abs(y - horizonY) < 2.0f) {
            // Use a fixed safe value near the horizon
            yOffset = (y < horizonY) ? -2.0f : 2.0f;
        } else {
            yOffset = static_cast<float>(y - horizonY);
        }
        
        // Calculate z with stabilized offset
        float z = DISTANCE_MULTIPLIER * (view.height - visplane.height) / 
                  (visplane.isFloor ? yOffset : -yOffset);
        
        // Skip if too close or too far
        if (z < 0.1f || z > 100.0f) continue;
        
        // Apply different depth biases based on plane type and distance from walls
        float depthBias = visplane.isFloor ? 0.1f : -0.1f;
        z += depthBias;
        
        // Process spans for this scanline
        int spanStart = -1; // Start of current span
        float spanStartU = 0.0f;
        float spanStartV = 0.0f;
        float spanStartZ = 0.0f;
        
        // Process this scanline from left to right
        for (int x = 0; x < m_width; x++) {
            // Skip if x is out of screen bounds
            if (x < 0 || x >= m_width) continue;
            
            // Check if this pixel is part of the visplane
            const auto& column = visplane.columns[x];
            
            // Add a 1-pixel overlap on visplane edges to prevent seams
            bool isInVisplane = false;
            if (column.yStart >= 0 && column.yEnd >= 0) {
                int yStart = column.yStart;
                int yEnd = column.yEnd;
                
                // Expand the visplane slightly at the edges to prevent seams
                if (x > 0 && x < m_width - 1) {
                    const auto& prevColumn = visplane.columns[x-1];
                    const auto& nextColumn = visplane.columns[x+1];
                    
                    // If adjacent columns have valid ranges, expand this one slightly
                    if (prevColumn.yStart >= 0 && prevColumn.yEnd >= 0) {
                        yStart = std::min(yStart, prevColumn.yStart);
                        yEnd = std::max(yEnd, prevColumn.yEnd);
                    }
                    
                    if (nextColumn.yStart >= 0 && nextColumn.yEnd >= 0) {
                        yStart = std::min(yStart, nextColumn.yStart);
                        yEnd = std::max(yEnd, nextColumn.yEnd);
                    }
                }
                
                isInVisplane = (y >= yStart && y <= yEnd);
            }
            
            if (isInVisplane) {
                // Calculate texture coordinates for this pixel
                // Convert screen coordinate to world coordinate with stable interpolation
                Vec2 worldPos = screenToWorld(x, y, z, view);
                
                // Calculate texture coordinates (simple tiling)
                float texU = worldPos.x * 0.1f;
                float texV = worldPos.y * 0.1f;
                
                // Wrap to [0,1]
                texU = texU - std::floor(texU);
                texV = texV - std::floor(texV);
                
                // Calculate perspective correction values
                float invZ = 1.0f / std::max(0.1f, std::abs(z));
                
                if (spanStart == -1) {
                    // Start a new span
                    spanStart = x;
                    spanStartU = texU;
                    spanStartV = texV;
                    spanStartZ = z;
                }
                
                // Draw the spans continuously to avoid seams
                if (x == m_width - 1 || 
                    x + 1 >= m_width || 
                    y < visplane.columns[x + 1].yStart || 
                    y > visplane.columns[x + 1].yEnd) {
                    
                    // End of span, create and render it
                    Span span;
                    span.y = y;
                    span.startX = spanStart;
                    span.endX = x;
                    span.startU = spanStartU;
                    span.startV = spanStartV;
                    span.endU = texU;
                    span.endV = texV;
                    span.startZ = spanStartZ;
                    span.endZ = z;
                    span.textureId = visplane.textureId;
                    span.lightLevel = visplane.lightLevel;
                    span.isFloor = visplane.isFloor;
                    
                    renderSpan(span);
                    
                    // Reset for next span
                    spanStart = -1;
                }
            } else {
                // Not part of visplane, end any active span
                if (spanStart != -1) {
                    // End of span, render it
                    Span span;
                    span.y = y;
                    span.startX = spanStart;
                    span.endX = x - 1;
                    span.startU = spanStartU;
                    span.startV = spanStartV;
                    span.endU = spanStartU + (spanStartU - spanStartV) * (x - 1 - spanStart) / (x - spanStart);
                    span.endV = spanStartV + (spanStartV - spanStartU) * (x - 1 - spanStart) / (x - spanStart);
                    span.startZ = spanStartZ;
                    span.endZ = spanStartZ;
                    span.textureId = visplane.textureId;
                    span.lightLevel = visplane.lightLevel;
                    span.isFloor = visplane.isFloor;
                    
                    renderSpan(span);
                    
                    // Reset for next span
                    spanStart = -1;
                }
            }
        }
    }
}

void Renderer::renderSpan(const Span& span) {
    // Early exits
    if (span.startX > span.endX) return;
    if (span.y < 0 || span.y >= m_height) return;
    
    // Clamp x range to screen
    int startX = std::max(0, span.startX);
    int endX = std::min(m_width - 1, span.endX);
    
    if (startX > endX) return;
    
    // Get texture
    int texIndex = span.textureId % m_textures.size();
    const Texture& tex = m_textures[texIndex];
    
    // Calculate step values for texture coordinates with improved stability
    float spanLength = static_cast<float>(span.endX - span.startX);
    if (spanLength < 1.0f) spanLength = 1.0f; // Prevent division by zero
    
    float uStep = (span.endU - span.startU) / spanLength;
    float vStep = (span.endV - span.startV) / spanLength;
    float zStep = (span.endZ - span.startZ) / spanLength;
    
    // Current texture coordinates with smooth interpolation
    float u = span.startU + (startX - span.startX) * uStep;
    float v = span.startV + (startX - span.startX) * vStep;
    float z = span.startZ + (startX - span.startX) * zStep;
    
    // Apply more stable perspective correction
    float baseFactor = (span.isFloor) ? 1.05f : 0.95f; // Slight bias to fix z-fighting
    
    // Calculate lighting factor (0-1)
    float lightFactor = std::min(1.0f, std::max(0.0f, span.lightLevel / 255.0f));
    
    // Draw the span one pixel at a time
    for (int x = startX; x <= endX; x++) {
        // Calculate stable texture coordinates
        float sampleU = u;
        float sampleV = v;
        
        // Wrap to [0,1]
        sampleU = sampleU - std::floor(sampleU);
        sampleV = sampleV - std::floor(sampleV);
        
        // Get texture color
        Color texColor = tex.sample(sampleU, sampleV);
        
        // Apply lighting
        Color finalColor = Color::blend(Color(0, 0, 0), texColor, lightFactor);
        
        // Apply distance fog
        float fogFactor = 1.0f - std::min(1.0f, z / 30.0f);
        finalColor = Color::blend(Color(0, 0, 0), finalColor, fogFactor);
        
        // Apply a consistent depth bias based on plane type
        float adjustedZ = z * baseFactor;
        
        // Draw the pixel with depth information
        drawPixelWithDepth(x, span.y, adjustedZ, finalColor);
        
        // Step to next pixel
        u += uStep;
        v += vStep;
        z += zStep;
    }
}

void Renderer::renderSprites(const BSPTree& bsp, const ViewPosition& view, const std::vector<Sprite>& sprites) {
    // Combine local and passed sprites
    std::vector<Sprite> allSprites = m_sprites;
    allSprites.insert(allSprites.end(), sprites.begin(), sprites.end());
    
    // If no sprites, nothing to render
    if (allSprites.empty()) {
        std::cout << "No sprites to render!" << std::endl;
        return;
    }
    
    std::cout << "Total sprites: " << allSprites.size() << std::endl;
    
    // Sort sprites from back to front
    std::vector<SpriteRenderData> sortedSprites = sortSprites(allSprites, view);
    
    std::cout << "Visible sprites after sorting: " << sortedSprites.size() << std::endl;
    
    // Render each sprite in order
    int renderedCount = 0;
    for (const auto& spriteData : sortedSprites) {
        std::cout << "Rendering sprite: " << spriteData.sprite->tag 
                  << " at position (" << spriteData.sprite->position.x << ", " << spriteData.sprite->position.y 
                  << "), texture ID: " << spriteData.sprite->getCurrentFrame().textureId 
                  << ", distance: " << spriteData.distance << std::endl;
        
        renderSprite(*spriteData.sprite, bsp, view, spriteData.distance);
        renderedCount++;
    }
    
    std::cout << "Actually rendered " << renderedCount << " sprites" << std::endl;
}

std::vector<SpriteRenderData> Renderer::sortSprites(const std::vector<Sprite>& sprites, const ViewPosition& view) const {
    std::vector<SpriteRenderData> sortedSprites;
    sortedSprites.reserve(sprites.size());
    
    // Calculate distance for each sprite and check visibility
    for (const Sprite& sprite : sprites) {
        if (!sprite.visible) {
            continue;
        }
        
        float distance;
        if (isSpriteVisible(sprite, view, distance)) {
            sortedSprites.emplace_back(&sprite, distance);
        }
    }
    
    // Sort by distance (back to front)
    std::sort(sortedSprites.begin(), sortedSprites.end());
    
    return sortedSprites;
}

bool Renderer::isSpriteVisible(const Sprite& sprite, const ViewPosition& view, float& distance) const {
    // Calculate vector from viewer to sprite
    Vec2 toSprite = sprite.position - view.position;
    
    // Calculate distance
    distance = toSprite.length();
    
    // If too close or too far, not visible
    if (distance < 0.1f || distance > 100.0f) {
        std::cout << "Sprite '" << sprite.tag << "' too close or too far: " << distance << std::endl;
        return false;
    }
    
    // Calculate angle to sprite
    float angle = std::atan2(toSprite.y, toSprite.x);
    
    // Normalize angle to [0, 2π)
    while (angle < 0) angle += 2 * PI;
    while (angle >= 2 * PI) angle -= 2 * PI;
    
    // Calculate angle difference
    float viewAngle = view.angle;
    while (viewAngle < 0) viewAngle += 2 * PI;
    while (viewAngle >= 2 * PI) viewAngle -= 2 * PI;
    
    float angleDiff = std::abs(angle - viewAngle);
    if (angleDiff > PI) {
        angleDiff = 2 * PI - angleDiff;
    }
    
    // Check if within field of view
    float halfFOV = view.fov * DEG_TO_RAD / 2.0f;
    bool isVisible = angleDiff <= halfFOV;
    
    if (!isVisible) {
        std::cout << "Sprite '" << sprite.tag << "' outside FOV. Angle diff: " << (angleDiff * 180.0f / PI) 
                  << " degrees, Half FOV: " << (halfFOV * 180.0f / PI) << " degrees" << std::endl;
    }
    
    return isVisible;
}

void Renderer::renderSprite(const Sprite& sprite, const BSPTree& bsp, const ViewPosition& view, float distance) {
    // Get the current frame
    const SpriteFrame& frame = sprite.getCurrentFrame();
    if (frame.textureId < 0 || frame.textureId >= static_cast<int>(m_textures.size())) {
        std::cout << "Skipping sprite '" << sprite.tag << "': Invalid texture ID " << frame.textureId << " (texture count: " << m_textures.size() << ")" << std::endl;
        return; // Invalid texture
    }
    
    // Get world height
    float spriteY = sprite.getWorldHeight(bsp);
    
    // Calculate sprite dimensions in world
    float spriteWidth = frame.width * sprite.scale;
    float spriteHeight = frame.height * sprite.scale;
    
    // Calculate sprite position in screen space
    Vec2 screenPos = worldToScreen(sprite.position, view);
    
    // If behind camera, don't render
    if (screenPos.x < 0) {
        std::cout << "Skipping sprite '" << sprite.tag << "': Behind camera (screen x: " << screenPos.x << ")" << std::endl;
        return;
    }
    
    // Calculate sprite vertical position on screen
    float spriteWorldY = spriteY + spriteHeight / 2.0f;
    float heightDifference = spriteWorldY - view.height;
    
    // Calculate projected sprite height
    float projectedHeight = calculateWallHeight(distance, spriteHeight);
    
    // Calculate sprite top and bottom on screen
    int centerY = m_height / 2 - static_cast<int>((heightDifference / distance) * DISTANCE_MULTIPLIER);
    int spriteTop = centerY - static_cast<int>(projectedHeight / 2);
    int spriteBottom = centerY + static_cast<int>(projectedHeight / 2);
    
    // Calculate sprite left and right on screen
    float projectedWidth = calculateWallHeight(distance, spriteWidth);
    int spriteLeft = static_cast<int>(screenPos.x - projectedWidth / 2);
    int spriteRight = static_cast<int>(screenPos.x + projectedWidth / 2);
    
    // Clamp to screen bounds
    int left = std::max(0, spriteLeft);
    int right = std::min(m_width - 1, spriteRight);
    int top = std::max(0, spriteTop);
    int bottom = std::min(m_height - 1, spriteBottom);
    
    // Get sprite texture
    const Texture& texture = m_textures[frame.textureId];
    
    // Calculate lighting factor (0-1)
    float lightFactor = std::min(1.0f, std::max(0.0f, sprite.lightLevel / 255.0f));
    
    // Apply distance fog
    float fogFactor = 1.0f - std::min(1.0f, distance / 30.0f);
    
    // Calculate inverse Z for perspective correction
    float invZ = 1.0f / std::max(0.1f, distance);
    
    // Draw the sprite
    for (int x = left; x <= right; x++) {
        // Calculate texture coordinate with perspective correction
        float screenX = static_cast<float>(x - spriteLeft) / (spriteRight - spriteLeft);
        if (sprite.flipped) {
            screenX = 1.0f - screenX;
        }
        
        // Apply perspective correction
        // For sprites, we want to maintain the billboard effect, so we apply a slight
        // perspective correction that keeps the overall billboard shape but reduces texture swimming
        float u = screenX;
        
        // Apply subtle perspective distortion to make the sprite feel more 3D
        // This simulates the curve you'd see on a true 3D object
        float distFromCenter = std::abs(screenX - 0.5f) * 2.0f; // 0 at center, 1 at edges
        float edgeDistance = distance * (1.0f + distFromCenter * 0.1f); // Slightly more distant at edges
        
        // Draw vertical stripe
        for (int y = top; y <= bottom; y++) {
            // Calculate texture coordinate with perspective correction
            float screenY = static_cast<float>(y - spriteTop) / (spriteBottom - spriteTop);
            float v = screenY;
            
            // Sample texture
            Color color = texture.sample(u, v);
            
            // Skip transparent pixels
            if (color.a < 128) {
                continue;
            }
            
            // Apply lighting
            Color litColor = Color::blend(Color(0, 0, 0), color, lightFactor);
            
            // Apply fog
            Color finalColor = Color::blend(Color(0, 0, 0), litColor, fogFactor);
            
            // Draw pixel with depth check - use slightly adjusted depth for curved billboard effect
            drawPixelWithDepth(x, y, edgeDistance, finalColor);
        }
    }
}

void Renderer::addSprite(const Sprite& sprite) {
    m_sprites.push_back(sprite);
}

void Renderer::clearSprites() {
    m_sprites.clear();
}

void Renderer::drawVerticalLine(int x, int y1, int y2, const Color& color) {
    if (x < 0 || x >= m_width) return;
    
    y1 = std::max(0, std::min(m_height - 1, y1));
    y2 = std::max(0, std::min(m_height - 1, y2));
    
    if (y2 < y1) std::swap(y1, y2);
    
    for (int y = y1; y <= y2; y++) {
        drawPixel(x, y, color);
    }
}

void Renderer::drawPixel(int x, int y, const Color& color) {
    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        m_frameBuffer[y * m_width + x] = color;
    }
}

void Renderer::drawHorizontalLine(int y, int x1, int x2, const Color& color) {
    if (y < 0 || y >= m_height) return;
    
    x1 = std::max(0, std::min(m_width - 1, x1));
    x2 = std::max(0, std::min(m_width - 1, x2));
    
    if (x2 < x1) std::swap(x1, x2);
    
    for (int x = x1; x <= x2; x++) {
        drawPixel(x, y, color);
    }
}

float Renderer::calculateWallHeight(float distance, float wallHeight) const {
    // DOOM-style perspective projection
    if (distance < 0.1f) distance = 0.1f; // Prevent division by zero
    
    return (DISTANCE_MULTIPLIER * wallHeight) / distance;
}

int Renderer::calculateScreenY(float projHeight, float offset) const {
    int centerY = m_height / 2;
    return centerY - static_cast<int>(projHeight * offset);
}

Vec2 Renderer::worldToScreen(const Vec2& worldPos, const ViewPosition& view) const {
    // Convert world space to camera space
    Vec2 cameraVec = worldPos - view.position;
    
    // Rotate to camera orientation
    float cosAngle = std::cos(-view.angle);
    float sinAngle = std::sin(-view.angle);
    float xCamera = cameraVec.x * cosAngle - cameraVec.y * sinAngle;
    float yCamera = cameraVec.x * sinAngle + cameraVec.y * cosAngle;
    
    // Project to screen space
    float halfFovRad = view.fov * 0.5f * DEG_TO_RAD;
    float aspectRatio = static_cast<float>(m_width) / m_height;
    
    // Prevent division by zero
    if (xCamera < 0.1f) xCamera = 0.1f;
    
    float screenX = m_width * (0.5f + yCamera / (xCamera * std::tan(halfFovRad) * 2.0f));
    float screenY = m_height * (0.5f - 1.0f / (xCamera * 2.0f));
    
    return Vec2(screenX, screenY);
}

Vec2 Renderer::screenToWorld(int x, int y, float z, const ViewPosition& view) const {
    // Calculate camera space position from screen position
    float screenX = (x - m_width / 2.0f) / (m_width / 2.0f);
    float screenY = (m_height / 2.0f - y) / (m_height / 2.0f);
    
    // View parameters
    float halfFovRad = view.fov * 0.5f * DEG_TO_RAD;
    float tanFov = std::tan(halfFovRad);
    
    // Convert screen position to world position
    float xCamera = z;
    float yCamera = screenX * z * tanFov * (static_cast<float>(m_width) / m_height);
    float zCamera = screenY * z * tanFov;
    
    // Rotate from camera space to world space
    float cosAngle = std::cos(view.angle);
    float sinAngle = std::sin(view.angle);
    
    float xWorld = view.position.x + xCamera * cosAngle - yCamera * sinAngle;
    float yWorld = view.position.y + xCamera * sinAngle + yCamera * cosAngle;
    
    return Vec2(xWorld, yWorld);
}

// Helper functions for Z-buffer operations
float Renderer::getDepth(int x, int y) const {
    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        return m_zBuffer[y * m_width + x];
    }
    return std::numeric_limits<float>::max();
}

void Renderer::setDepth(int x, int y, float depth) {
    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        m_zBuffer[y * m_width + x] = depth;
    }
}

bool Renderer::isPixelVisible(int x, int y, float depth) const {
    // Add a larger epsilon value for z-fighting prevention
    const float DEPTH_EPSILON = 0.005f;
    
    // Get current z-buffer depth
    float currentDepth = getDepth(x, y);
    
    // If depths are very close (potential z-fighting), use additional criteria
    if (std::abs(depth - currentDepth) < 0.02f) {
        // Prefer the depth that gives a more stable pattern
        // Use a checkerboard pattern for stability near portals
        bool isEvenX = (x % 2) == 0;
        bool isEvenY = (y % 2) == 0;
        
        // If we're in a z-fighting situation, use the checkerboard to select
        if (isEvenX == isEvenY) {
            // For even pattern squares, prefer the greater depth (further back)
            return depth < (currentDepth - DEPTH_EPSILON * 2.0f);
        } else {
            // For odd pattern squares, prefer the lesser depth (closer)
            return depth < (currentDepth - DEPTH_EPSILON * 0.5f);
        }
    }
    
    // Normal case: use standard depth test with epsilon
    return depth < (currentDepth - DEPTH_EPSILON);
}

void Renderer::drawPixelWithDepth(int x, int y, float depth, const Color& color) {
    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        if (isPixelVisible(x, y, depth)) {
            m_frameBuffer[y * m_width + x] = color;
            setDepth(x, y, depth);
        }
    }
}

// New skybox rendering function
void Renderer::renderSkybox(const ViewPosition& view, float deltaTime) {
    // Update the skybox state (sun position, colors based on time of day)
    m_skybox.update(deltaTime);
    
    // Calculate the visible angle range
    float halfFov = view.fov * 0.5f * DEG_TO_RAD;
    
    // Render the sky gradient for the upper half of the screen
    int horizonY = m_height / 2;
    
    // Performance optimization for sky gradient
    bool performanceMode = (m_skybox.maxViewDistance < 20.0f);
    int lineStep = performanceMode ? 2 : 1; // Draw every other line in performance mode
    
    // Draw the sky gradient from top to horizon
    for (int y = 0; y < horizonY; y += lineStep) {
        // Calculate interpolation factor (0 at horizon, 1 at top)
        float t = static_cast<float>(y) / horizonY;
        t = 1.0f - t;  // Invert to go from top to horizon
        
        // Blend between zenith and horizon colors
        Color skyColor = Color::blend(m_skybox.horizonColor, m_skybox.zenithColor, t);
        
        // Draw a horizontal line with this color
        drawHorizontalLine(y, 0, m_width - 1, skyColor);
        
        // In performance mode, also fill the next line to maintain smoothness
        if (performanceMode && y + 1 < horizonY) {
            drawHorizontalLine(y + 1, 0, m_width - 1, skyColor);
        }
        
        // Set depth to maximum for the sky (use step for efficiency)
        for (int x = 0; x < m_width; x += 4) {  // Process in blocks of 4 for efficiency
            for (int i = 0; i < 4 && x + i < m_width; i++) {
                setDepth(x + i, y, std::numeric_limits<float>::max());
                if (performanceMode && y + 1 < horizonY) {
                    setDepth(x + i, y + 1, std::numeric_limits<float>::max());
                }
            }
        }
    }
    
    // If in performance mode and dynamic sky is off, skip sun rendering
    if (performanceMode && !m_skybox.dynamicSky) {
        return;
    }
    
    // Prepare to place the sun in the sky
    // Calculate sun position in screen space
    float sunScreenAngle = m_skybox.sunAngle - view.angle;
    
    // Normalize angle to [-π, π]
    while (sunScreenAngle > PI) sunScreenAngle -= 2.0f * PI;
    while (sunScreenAngle < -PI) sunScreenAngle += 2.0f * PI;
    
    // Check if sun is potentially visible (within field of view plus some margin)
    if (std::abs(sunScreenAngle) < halfFov + DEG_TO_RAD * 10.0f) {
        // Calculate x position based on angle
        float normalizedAngle = sunScreenAngle / halfFov;  // -1 to 1 range
        float screenX = m_width / 2 * (1.0f + normalizedAngle);
        
        // Calculate y position based on height
        float sunHeightFactor = m_skybox.sunHeight;  // -1 to 1, where 0 is horizon
        float screenY = horizonY * (1.0f - sunHeightFactor * 0.8f);  // Scale by 0.8 to keep it in view
        
        // Only draw glow in high quality mode
        if (!performanceMode) {
            // Draw the sun glow (halo effect)
            float glowSize = m_skybox.sunSize * m_skybox.sunGlowSize;
            drawSun(screenX, screenY, glowSize, m_skybox.sunGlowColor, 0.5f);
        }
        
        // Draw the sun itself
        drawSun(screenX, screenY, m_skybox.sunSize, m_skybox.sunColor, 1.0f);
    }
}

// Draw the sun as a glowing circle
void Renderer::drawSun(float screenX, float screenY, float sizeDegrees, const Color& color, float intensity) {
    // Convert sun size from degrees to pixels
    // Assuming FOV is mapped to screen width
    float sizePixels = (sizeDegrees / 90.0f) * m_width * 0.5f;
    
    // Calculate sun boundaries
    int left = std::max(0, static_cast<int>(screenX - sizePixels));
    int right = std::min(m_width - 1, static_cast<int>(screenX + sizePixels));
    int top = std::max(0, static_cast<int>(screenY - sizePixels));
    int bottom = std::min(m_height - 1, static_cast<int>(screenY + sizePixels));
    
    float radiusSquared = sizePixels * sizePixels;
    
    // Performance optimization: Draw with larger step size when in performance mode
    int step = (m_skybox.maxViewDistance < 20.0f) ? 2 : 1; // Skip pixels in performance mode
    
    // Draw the sun
    for (int y = top; y <= bottom; y += step) {
        for (int x = left; x <= right; x += step) {
            // Calculate distance from center
            float dx = x - screenX;
            float dy = y - screenY;
            float distanceSquared = dx * dx + dy * dy;
            
            if (distanceSquared <= radiusSquared) {
                // Calculate brightness based on distance from center
                float dist = std::sqrt(distanceSquared);
                float brightness = 1.0f - (dist / sizePixels);
                
                // Apply intensity
                brightness *= intensity;
                
                // Add to existing color (additive blending)
                if (brightness > 0.05f) {  // Threshold to avoid faint edges
                    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
                        Color existingColor = m_frameBuffer[y * m_width + x];
                        
                        // Blend with existing color (additive for sun effect)
                        Color blendedColor(
                            std::min(255, existingColor.r + static_cast<int>(color.r * brightness)),
                            std::min(255, existingColor.g + static_cast<int>(color.g * brightness)),
                            std::min(255, existingColor.b + static_cast<int>(color.b * brightness))
                        );
                        
                        m_frameBuffer[y * m_width + x] = blendedColor;
                        
                        // In performance mode with step > 1, also fill adjacent pixels to reduce pixelation
                        if (step > 1) {
                            // Fill adjacent pixels in a 2x2 block
                            if (x + 1 < m_width && y + 1 < m_height) {
                                m_frameBuffer[(y) * m_width + (x+1)] = blendedColor;
                                m_frameBuffer[(y+1) * m_width + (x)] = blendedColor;
                                m_frameBuffer[(y+1) * m_width + (x+1)] = blendedColor;
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace PureDoom 
