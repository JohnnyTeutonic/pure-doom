#include "Renderer.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <limits>

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
    // In a real implementation, you'd load an image file
    // For this example, we'll just generate a pattern
    m_width = 64;
    m_height = 64;
    m_pixels.resize(m_width * m_height);
    generateCheckerboard();
    std::cout << "Generated pattern texture instead of loading file: " << filename << std::endl;
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

Renderer::~Renderer() = default;

void Renderer::initialize() {
    loadTextures();
    loadSpriteTextures();
    clearBuffers();
}

void Renderer::loadTextures() {
    // Create some basic textures for testing
    m_textures.push_back(Texture(64, 64)); // Default checkerboard
    
    // In a real implementation, you'd load various wall textures
    // For now, we'll create some basic patterns
    for (int i = 0; i < 10; i++) {
        Texture tex(64, 64);
        m_textures.push_back(tex);
    }
    
    std::cout << "Loaded " << m_textures.size() << " textures" << std::endl;
}

void Renderer::loadSpriteTextures() {
    // In a real implementation, you'd load sprite textures here
    // For now, we'll create some basic patterns for testing

    // List current textures
    std::cout << "Before loading sprite textures, we have " << m_textures.size() << " textures" << std::endl;

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
    int blueTextureIndex = m_textures.size() - 1;
    
    // Add a simple item sprite (gem)
    Texture itemTexture(64, 64);
    width = itemTexture.width();
    height = itemTexture.height();
    
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
    int greenTextureIndex = m_textures.size() - 1;
    
    // Create a simple enemy sprite (red diamond)
    Texture enemyTexture(64, 64);
    width = enemyTexture.width();
    height = enemyTexture.height();
    
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
    int redTextureIndex = m_textures.size() - 1;
    
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
    
    // Cast a ray for each column of the screen
    for (int x = 0; x < m_width; x++) {
        // Calculate ray angle
        float rayAngle = view.angle - halfFov + angleStep * x;
        
        // Normalize angle to [0, 2π)
        while (rayAngle < 0) rayAngle += 2 * PI;
        while (rayAngle >= 2 * PI) rayAngle -= 2 * PI;
        
        // Create ray direction vector
        Vec2 rayDir(std::cos(rayAngle), std::sin(rayAngle));
        
        // Cast ray and get collision info
        CollisionInfo collision = bsp.castRay(view.position, rayDir, 100.0f);
        
        if (collision.collision) {
            // Find which sector the wall belongs to
            int sectorId = collision.sectorId;
            if (sectorId >= 0 && sectorId < static_cast<int>(bsp.getSectors().size())) {
                const Sector& sector = bsp.getSectors()[sectorId];
                
                // Create a wall slice for rendering
                WallSlice slice;
                slice.x = x;
                slice.distance = collision.distance;
                
                // Correct for fisheye effect
                float correctedDistance = collision.distance * std::cos(rayAngle - view.angle);
                slice.distance = correctedDistance;
                
                // Get wall height and calculate screen height
                float wallHeight = sector.ceilingHeight - sector.floorHeight;
                slice.height = calculateWallHeight(correctedDistance, wallHeight);
                
                // Calculate texture coordinate based on hit point
                Vec2 wallStart, wallEnd;
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
                }
                
                // Apply light level from sector
                slice.lightLevel = sector.lightLevel;
                
                // Render the wall slice
                renderWallSlice(slice);
                
                // Update wall extents for this column
                int centerY = m_height / 2;
                int wallTop = centerY - static_cast<int>(slice.height / 2);
                int wallBottom = centerY + static_cast<int>(slice.height / 2);
                
                // Clamp to screen bounds
                wallTop = std::max(0, wallTop);
                wallBottom = std::min(m_height - 1, wallBottom);
                
                // Store wall extents for this column
                m_wallExtents[x].top = wallTop;
                m_wallExtents[x].bottom = wallBottom;
                
                // Add the floor visplane for this sector (will check if already exists)
                if (wallBottom < m_height - 1) {
                    bool found = false;
                    for (auto& plane : m_visplanes) {
                        if (plane.isFloor && std::abs(plane.height - sector.floorHeight) < 0.001f &&
                            plane.textureId == sector.floorTextureId) {
                            // Update existing visplane
                            plane.minX = std::min(plane.minX, x);
                            plane.maxX = std::max(plane.maxX, x);
                            plane.top[x] = wallBottom + 1;
                            plane.bottom[x] = m_height - 1;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        // Create new visplane
                        Visplane plane(m_width, sector.floorHeight, sector.floorTextureId, sector.lightLevel, true);
                        plane.minX = x;
                        plane.maxX = x;
                        plane.top[x] = wallBottom + 1;
                        plane.bottom[x] = m_height - 1;
                        m_visplanes.push_back(plane);
                    }
                }
                
                // Add the ceiling visplane for this sector
                if (wallTop > 0) {
                    bool found = false;
                    for (auto& plane : m_visplanes) {
                        if (!plane.isFloor && std::abs(plane.height - sector.ceilingHeight) < 0.001f &&
                            plane.textureId == sector.ceilingTextureId) {
                            // Update existing visplane
                            plane.minX = std::min(plane.minX, x);
                            plane.maxX = std::max(plane.maxX, x);
                            plane.top[x] = 0;
                            plane.bottom[x] = wallTop - 1;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        // Create new visplane
                        Visplane plane(m_width, sector.ceilingHeight, sector.ceilingTextureId, sector.lightLevel, false);
                        plane.minX = x;
                        plane.maxX = x;
                        plane.top[x] = 0;
                        plane.bottom[x] = wallTop - 1;
                        m_visplanes.push_back(plane);
                    }
                }
            }
        }
    }
}

void Renderer::renderWallSlice(const WallSlice& slice) {
    if (slice.x < 0 || slice.x >= m_width) return;
    
    // Calculate wall top and bottom on screen
    int centerY = m_height / 2;
    int wallTop = centerY - static_cast<int>(slice.height / 2);
    int wallBottom = centerY + static_cast<int>(slice.height / 2);
    
    // Clamp to screen bounds
    wallTop = std::max(0, wallTop);
    wallBottom = std::min(m_height - 1, wallBottom);
    
    // Get the texture for this wall
    int texIndex = slice.textureId % m_textures.size();
    const Texture& tex = m_textures[texIndex];
    
    // Calculate lighting factor (0-1)
    float lightFactor = std::min(1.0f, std::max(0.0f, slice.lightLevel / 255.0f));
    
    // Draw the wall slice
    for (int y = wallTop; y <= wallBottom; y++) {
        // Calculate texture coordinate V
        float yNorm = static_cast<float>(y - wallTop) / (wallBottom - wallTop);
        Color texColor = tex.sample(slice.texCoordU, yNorm);
        
        // Apply lighting
        Color finalColor = Color::blend(Color(0, 0, 0), texColor, lightFactor);
        
        // Apply distance fog
        float fogFactor = 1.0f - std::min(1.0f, slice.distance / 30.0f);
        finalColor = Color::blend(Color(0, 0, 0), finalColor, fogFactor);
        
        // Draw the pixel with depth information
        drawPixelWithDepth(slice.x, y, slice.distance, finalColor);
    }
}

void Renderer::renderFloorAndCeilingSpans(const BSPTree& bsp, const ViewPosition& view) {
    // Process each visplane and render it
    for (const auto& visplane : m_visplanes) {
        renderVisplane(visplane, view);
    }
}

void Renderer::renderVisplane(const Visplane& visplane, const ViewPosition& view) {
    // Current viewer height relative to the plane
    float planeZ = visplane.isFloor ? 
                  (visplane.height - view.height) : 
                  (visplane.height - view.height);
    
    // Skip if the viewer is too close to the plane
    if (std::abs(planeZ) < 0.001f) return;
    
    // Get texture
    int texIndex = visplane.textureId % m_textures.size();
    const Texture& tex = m_textures[texIndex];
    
    // Calculate lighting factor (0-1)
    float lightFactor = std::min(1.0f, std::max(0.0f, visplane.lightLevel / 255.0f));
    
    // Screen center Y coordinate
    int centerY = m_height / 2;
    
    // Process each scanline (y-coordinate) of the visplane
    for (int y = 0; y < m_height; y++) {
        // Skip if this isn't where we expect the plane to be
        bool shouldBeFloor = (y >= centerY);
        if (visplane.isFloor != shouldBeFloor) continue;
        
        // Calculate the scale factor for the perspective
        // (DOOM used a lookup table for this calculation)
        float yDistFromCenter = static_cast<float>(y - centerY);
        if (yDistFromCenter == 0.0f) continue; // Skip the center row
        
        // DOOM's fixed point math approximated:
        // The farther a pixel is from the center of the screen,
        // the greater the z-coordinate in the 3D world
        float scale = DISTANCE_MULTIPLIER / yDistFromCenter;
        
        // Skip horizontal scan if scale is out of range
        if (scale <= 0.01f) continue;
        
        // For floor, scale is negative (below view), for ceiling positive (above view)
        scale = visplane.isFloor ? -scale : scale;
        
        // Calculate the distance to the floor/ceiling for this scanline
        float z = planeZ * scale;
        
        // Skip if too close or too far
        if (z < 0.1f || z > 100.0f) continue;
        
        // Process spans for this scanline
        int spanStart = -1; // Start of current span
        float spanStartU = 0.0f;
        float spanStartV = 0.0f;
        float spanStartZ = 0.0f;
        
        // Process this scanline from left to right
        for (int x = visplane.minX; x <= visplane.maxX; x++) {
            // Skip if x is out of screen bounds
            if (x < 0 || x >= m_width) continue;
            
            // Check if this pixel is part of the visplane
            if (y >= visplane.top[x] && y <= visplane.bottom[x]) {
                // Calculate texture coordinates for this pixel
                // Convert screen coordinate to world coordinate
                Vec2 worldPos = screenToWorld(x, y, z, view);
                
                // Calculate texture coordinates (simple tiling)
                float texU = worldPos.x * 0.1f;
                float texV = worldPos.y * 0.1f;
                
                // Wrap to [0,1]
                texU = texU - std::floor(texU);
                texV = texV - std::floor(texV);
                
                if (spanStart == -1) {
                    // Start a new span
                    spanStart = x;
                    spanStartU = texU;
                    spanStartV = texV;
                    spanStartZ = z;
                }
                
                // If we're at the end of the screen or the end of a span, render it
                if (x == visplane.maxX || x + 1 >= m_width || 
                    y < visplane.top[x + 1] || y > visplane.bottom[x + 1]) {
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
                    // End of span, create and render it
                    Span span;
                    span.y = y;
                    span.startX = spanStart;
                    span.endX = x - 1;
                    span.startU = spanStartU;
                    span.startV = spanStartV;
                    span.endU = spanStartU + (spanStartU - spanStartV) * (x - 1 - spanStart) / (x - spanStart);
                    span.endV = spanStartV + (spanStartV - spanStartU) * (x - 1 - spanStart) / (x - spanStart);
                    span.startZ = spanStartZ;
                    span.endZ = spanStartZ + (z - spanStartZ) * (x - 1 - spanStart) / (x - spanStart);
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
    
    // Calculate step values for texture coordinates
    float length = static_cast<float>(span.endX - span.startX);
    if (length < 0.001f) length = 0.001f; // Prevent division by zero
    
    float uStep = (span.endU - span.startU) / length;
    float vStep = (span.endV - span.startV) / length;
    float zStep = (span.endZ - span.startZ) / length;
    
    // Current texture coordinates and depth
    float u = span.startU + (startX - span.startX) * uStep;
    float v = span.startV + (startX - span.startX) * vStep;
    float z = span.startZ + (startX - span.startX) * zStep;
    
    // Calculate lighting factor (0-1)
    float lightFactor = std::min(1.0f, std::max(0.0f, span.lightLevel / 255.0f));
    
    // Draw the span one pixel at a time
    for (int x = startX; x <= endX; x++) {
        // Get texture color
        Color texColor = tex.sample(u, v);
        
        // Apply lighting
        Color finalColor = Color::blend(Color(0, 0, 0), texColor, lightFactor);
        
        // Apply distance fog
        float fogFactor = 1.0f - std::min(1.0f, z / 30.0f);
        finalColor = Color::blend(Color(0, 0, 0), finalColor, fogFactor);
        
        // Draw the pixel with depth information
        drawPixelWithDepth(x, span.y, z, finalColor);
        
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
    
    // Draw the sprite
    for (int x = left; x <= right; x++) {
        // Calculate texture coordinate
        float u = static_cast<float>(x - spriteLeft) / (spriteRight - spriteLeft);
        if (sprite.flipped) {
            u = 1.0f - u;
        }
        
        // Draw vertical stripe
        for (int y = top; y <= bottom; y++) {
            // Calculate texture coordinate
            float v = static_cast<float>(y - spriteTop) / (spriteBottom - spriteTop);
            
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
            
            // Draw pixel with depth check
            drawPixelWithDepth(x, y, distance, finalColor);
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
    return depth < getDepth(x, y);
}

void Renderer::drawPixelWithDepth(int x, int y, float depth, const Color& color) {
    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        if (isPixelVisible(x, y, depth)) {
            m_frameBuffer[y * m_width + x] = color;
            setDepth(x, y, depth);
        }
    }
}

} // namespace PureDoom 
