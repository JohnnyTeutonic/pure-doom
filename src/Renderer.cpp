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
    m_zBuffer.resize(width, std::numeric_limits<float>::max());
    m_wallExtents.resize(width);
}

Renderer::~Renderer() = default;

void Renderer::initialize() {
    loadTextures();
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

void Renderer::clearBuffers() {
    // Clear the frame buffer to black
    std::fill(m_frameBuffer.begin(), m_frameBuffer.end(), Color(0, 0, 0));
    
    // Reset the z-buffer
    std::fill(m_zBuffer.begin(), m_zBuffer.end(), std::numeric_limits<float>::max());
}

void Renderer::renderFrame(const BSPTree& bsp, const ViewPosition& view) {
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
                
                // Update z-buffer
                m_zBuffer[x] = correctedDistance;
                
                // Update visplanes - create/update floor and ceiling planes
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
        
        // Draw the pixel
        drawPixel(slice.x, y, finalColor);
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
        // Skip if there's a wall in front
        if (z > m_zBuffer[x]) {
            // Get texture color
            Color texColor = tex.sample(u, v);
            
            // Apply lighting
            Color finalColor = Color::blend(Color(0, 0, 0), texColor, lightFactor);
            
            // Apply distance fog
            float fogFactor = 1.0f - std::min(1.0f, z / 30.0f);
            finalColor = Color::blend(Color(0, 0, 0), finalColor, fogFactor);
            
            // Draw the pixel
            drawPixel(x, span.y, finalColor);
        }
        
        // Step to next pixel
        u += uStep;
        v += vStep;
        z += zStep;
    }
}

// Backward compatible simpler method (not span-based)
void Renderer::renderFloorAndCeilingSimple(const BSPTree& bsp, const ViewPosition& view) {
    // Find which sector the viewer is in
    int sectorId = bsp.findSector(view.position);
    if (sectorId < 0 || sectorId >= static_cast<int>(bsp.getSectors().size())) return;
    
    const Sector& sector = bsp.getSectors()[sectorId];
    
    // Basic colors for floor and ceiling
    Color floorColor(50, 50, 150);    // Dark blue floor
    Color ceilingColor(150, 150, 150); // Light gray ceiling
    
    // Adjust by sector light level
    float lightFactor = std::min(1.0f, std::max(0.0f, sector.lightLevel / 255.0f));
    floorColor = Color::blend(Color(0, 0, 0), floorColor, lightFactor);
    ceilingColor = Color::blend(Color(0, 0, 0), ceilingColor, lightFactor);
    
    // Screen center Y
    int centerY = m_height / 2;
    
    // Draw simple floor and ceiling
    for (int x = 0; x < m_width; x++) {
        // Get the wall height from the z-buffer
        float distance = m_zBuffer[x];
        
        // If there's no wall hit at this column, fill the entire column
        if (distance >= std::numeric_limits<float>::max() - 1.0f) {
            for (int y = 0; y < centerY; y++) {
                drawPixel(x, y, ceilingColor);
            }
            for (int y = centerY; y < m_height; y++) {
                drawPixel(x, y, floorColor);
            }
            continue;
        }
        
        // Calculate the wall height to determine where to start floor/ceiling
        float wallHeight = sector.ceilingHeight - sector.floorHeight;
        float projectedHeight = calculateWallHeight(distance, wallHeight);
        int wallTop = centerY - static_cast<int>(projectedHeight / 2);
        int wallBottom = centerY + static_cast<int>(projectedHeight / 2);
        
        // Draw ceiling
        for (int y = 0; y < wallTop; y++) {
            drawPixel(x, y, ceilingColor);
        }
        
        // Draw floor
        for (int y = wallBottom + 1; y < m_height; y++) {
            drawPixel(x, y, floorColor);
        }
    }
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

} // namespace PureDoom 