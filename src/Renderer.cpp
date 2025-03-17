#include "Renderer.h"
#include "TextureLoader.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <chrono>
#include <thread>

// Include CUDA renderer
#include "RendererCuda.h"

namespace PureDoom {

// Color utilities - only use these implementations when not compiling with CUDA
#if !defined(__CUDACC__) && !defined(ENABLE_CUDA)
namespace CPUImpl {
// Helper functions without Color:: qualification
static Color fromHSV_impl(float h, float s, float v) {
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

static Color blend_impl(const Color& c1, const Color& c2, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    float invT = 1.0f - t;
    
    return Color(
        static_cast<uint8_t>(c1.r * invT + c2.r * t),
        static_cast<uint8_t>(c1.g * invT + c2.g * t),
        static_cast<uint8_t>(c1.b * invT + c2.b * t),
        static_cast<uint8_t>(c1.a * invT + c2.a * t)
    );
}
} // namespace CPUImpl

// Forward the static method calls to the implementation
Color Color::fromHSV(float h, float s, float v) {
    return CPUImpl::fromHSV_impl(h, s, v);
}

Color Color::blend(const Color& c1, const Color& c2, float t) {
    return CPUImpl::blend_impl(c1, c2, t);
}
#endif

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
    m_frameBuffer.resize(width * height, Color(0, 0, 0));
    m_zBuffer.resize(width * height, std::numeric_limits<float>::infinity());
    m_wallExtents.resize(width, WallExtent());
    
    // Initialize GPU acceleration flag
    m_gpuAccelerationEnabled = true;
    
    // Set up minimap defaults
    m_minimapEnabled = true;
    m_minimapSize = std::min(width, height) / 2.5;  // Even larger minimap (was 1/3)
    m_minimapX = width - m_minimapSize - 10;     // Right corner
    m_minimapY = 10;                           // Top corner
    m_minimapScale = 0.8f;                     // Increased scale for better visibility (was 0.6f)
    
    // Load textures
    loadTextures();
    
    // Load sprite textures
    loadSpriteTextures();
}

Renderer::~Renderer() {
    // Clean up TextureLoader
    TextureLoader::shutdown();
    
    // Free any resources
    m_textures.clear();
    m_sprites.clear();
}

// Move constructor
Renderer::Renderer(Renderer&& other) noexcept
    : m_width(other.m_width), 
      m_height(other.m_height),
      m_frameBuffer(std::move(other.m_frameBuffer)),
      m_zBuffer(std::move(other.m_zBuffer)),
      m_textures(std::move(other.m_textures)),
      m_sprites(std::move(other.m_sprites)),
      m_skybox(std::move(other.m_skybox)),
      m_gpuAccelerationEnabled(other.m_gpuAccelerationEnabled),
      m_cudaRenderer(std::move(other.m_cudaRenderer)),
      m_wallExtents(std::move(other.m_wallExtents)),
      m_visplanes(std::move(other.m_visplanes)),
      m_minimapEnabled(other.m_minimapEnabled),
      m_minimapSize(other.m_minimapSize),
      m_minimapX(other.m_minimapX),
      m_minimapY(other.m_minimapY),
      m_minimapScale(other.m_minimapScale) {
}

// Move assignment operator
Renderer& Renderer::operator=(Renderer&& other) noexcept {
    if (this != &other) {
        m_width = other.m_width;
        m_height = other.m_height;
        m_frameBuffer = std::move(other.m_frameBuffer);
        m_zBuffer = std::move(other.m_zBuffer);
        m_textures = std::move(other.m_textures);
        m_sprites = std::move(other.m_sprites);
        m_skybox = std::move(other.m_skybox);
        m_gpuAccelerationEnabled = other.m_gpuAccelerationEnabled;
        m_cudaRenderer = std::move(other.m_cudaRenderer);
        m_wallExtents = std::move(other.m_wallExtents);
        m_visplanes = std::move(other.m_visplanes);
        m_minimapEnabled = other.m_minimapEnabled;
        m_minimapSize = other.m_minimapSize;
        m_minimapX = other.m_minimapX;
        m_minimapY = other.m_minimapY;
        m_minimapScale = other.m_minimapScale;
    }
    return *this;
}

void Renderer::initialize() {
    // Initialize texture loader
    if (!TextureLoader::initialize()) {
        std::cerr << "Failed to initialize texture loader" << std::endl;
    }
    
    // Load textures and sprites if not already loaded
    if (m_textures.empty()) {
        loadTextures();
    }
    if (m_sprites.empty()) {
        loadSpriteTextures();
    }
    
    // Register DOOM dungeon textures with the renderer
    // This ensures that the texture IDs in the BSP sectors match the renderer's texture vector
    TextureLoader::registerDoomTexturesWithRenderer(this);
    std::cout << "Total textures after registering DOOM textures: " << m_textures.size() << std::endl;
    
    // Flag that we need to upload textures to GPU on next render if enabled
    m_texturesUploaded = false;
    m_toggleGPU = true;
    
    clearBuffers();
    
    // Initialize CUDA renderer if GPU acceleration is enabled
    #if defined(ENABLE_CUDA)
    if (m_gpuAccelerationEnabled) {
        try {
            m_cudaRenderer = std::make_unique<RendererCuda>(m_width, m_height);
            if (!m_cudaRenderer->initialize()) {
                std::cout << "CUDA initialization failed. Using CPU rendering only." << std::endl;
                m_cudaRenderer.reset();
                m_gpuAccelerationEnabled = false;
            } else {
                std::cout << "CUDA acceleration enabled for rendering." << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "CUDA renderer initialization failed: " << e.what() << std::endl;
            m_cudaRenderer.reset();
            m_gpuAccelerationEnabled = false;
        }
    }
    #else
    m_gpuAccelerationEnabled = false;
    #endif
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
    
    // Upload textures to GPU immediately if GPU renderer is available
    #if defined(ENABLE_CUDA)
    if (m_gpuAccelerationEnabled && m_cudaRenderer) {
        std::cout << "Uploading " << m_textures.size() << " textures to GPU" << std::endl;
        m_cudaRenderer->uploadTextures(m_textures);
    }
    #endif
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
        std::shared_ptr<Texture> blueTex = TextureLoader::createProceduralTexture(64, 64, "blue");
        if (blueTex) {
            m_textures.push_back(*blueTex);
            blueTextureIndex = m_textures.size() - 1;
        }
    }
    
    if (greenTextureIndex == -1) {
        std::shared_ptr<Texture> greenTex = TextureLoader::createProceduralTexture(64, 64, "green");
        if (greenTex) {
            m_textures.push_back(*greenTex);
            greenTextureIndex = m_textures.size() - 1;
        }
    }
    
    if (redTextureIndex == -1) {
        std::shared_ptr<Texture> redTex = TextureLoader::createProceduralTexture(64, 64, "red");
        if (redTex) {
            m_textures.push_back(*redTex);
            redTextureIndex = m_textures.size() - 1;
        }
    }
    
    std::cout << "After loading sprite textures, we have " << m_textures.size() << " textures" << std::endl;
    std::cout << "Sprite texture indices - Blue: " << blueTextureIndex << ", Green: " << greenTextureIndex << ", Red: " << redTextureIndex << std::endl;
    
    // Upload the updated textures to GPU if available
    #if defined(ENABLE_CUDA)
    if (m_gpuAccelerationEnabled && m_cudaRenderer) {
        std::cout << "Uploading sprite textures to GPU" << std::endl;
        m_cudaRenderer->uploadTextures(m_textures);
    }
    #endif
}

void Renderer::clearBuffers() {
    // Clear the frame buffer to black
    std::fill(m_frameBuffer.begin(), m_frameBuffer.end(), Color(0, 0, 0));
    
    // Set Z-buffer to far distance
    std::fill(m_zBuffer.begin(), m_zBuffer.end(), 1.0f);
    
    // Reset wall extents for this frame
    for (auto& extent : m_wallExtents) {
        extent.top = m_height;
        extent.bottom = 0;
    }
    
    // Clear visplanes
    m_visplanes.clear();
}

// Render a frame
void Renderer::renderFrame(const BSPTree& bsp, const ViewPosition& view, const std::vector<Sprite>& sprites) {
    // Store the view position for minimap rendering
    m_viewPosition = view;
    m_playerPos = view.position;
    
    // Clear the frame buffer
    clearBuffers();
    
    // Render the skybox
    renderSkybox(view, 0.0f);
    
    // Render the BSP tree
    renderBSP(bsp, view);
    
    // Render platforms
    renderPlatforms(bsp, view);
    
    // Render sprites
    renderSprites(bsp, view, sprites);
    
    // Render minimap if enabled
    if (m_minimapEnabled) {
        renderMinimap(bsp, view);
    }
}

// Implementation with different parameter order (for internal use)
void Renderer::renderFrame(const BSPTree& bsp, std::vector<Sprite>& sprites, float deltaTime) {
    // Create a view position from the player position
    ViewPosition view;
    view.position = m_playerPos;
    view.angle = m_viewPosition.angle;
    view.fov = m_viewPosition.fov;
    view.height = m_viewPosition.height;
    
    // Update the skybox
    m_skybox.update(deltaTime);
    
    // Check if GPU acceleration is enabled and available
    if (m_gpuAccelerationEnabled && m_cudaRenderer) {
        try {
            // Upload textures to GPU if needed
            if (!m_texturesUploaded) {
                m_cudaRenderer->uploadTextures(m_textures);
                m_texturesUploaded = true;
            }
            
            // Set the skybox in the CUDA renderer
            m_cudaRenderer->setSkybox(m_skybox);
            
            // Render using CUDA
            m_cudaRenderer->renderFrame(bsp, view, sprites, deltaTime);
            
            // Render minimap if enabled (CPU-side)
            if (m_minimapEnabled) {
                renderMinimap(bsp, view);
            }
            
            return;
        }
        catch (const std::exception& e) {
            std::cerr << "CUDA rendering failed: " << e.what() << std::endl;
            std::cerr << "Falling back to CPU rendering." << std::endl;
            m_gpuAccelerationEnabled = false;
        }
    }
    
    // CPU rendering fallback
    renderCPU(bsp, sprites, deltaTime);
}

// CPU-only rendering (fallback when GPU rendering fails)
void Renderer::renderCPU(const BSPTree& bsp, std::vector<Sprite>& sprites, float deltaTime) {
    // Create a view position from the player position
    ViewPosition view;
    view.position = m_playerPos;
    view.angle = m_viewPosition.angle;
    view.fov = m_viewPosition.fov;
    view.height = m_viewPosition.height;
    
    // Clear the frame buffer
    clearBuffers();
    
    // Render the skybox
    renderSkybox(view, deltaTime);
    
    // Render the BSP tree
    renderBSP(bsp, view);
    
    // Render platforms
    renderPlatforms(bsp, view);
    
    // Render sprites
    renderSprites(bsp, view, sprites);
    
    // Render minimap if enabled
    if (m_minimapEnabled) {
        renderMinimap(bsp, view);
    }
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
                        
                        // Store adjacent sector heights
                        slice.adjacentCeilingHeight = adjacentSector.ceilingHeight;
                        slice.adjacentFloorHeight = adjacentSector.floorHeight;
                        
                        // Check for height differences between sectors
                        float ceilingDiff = sector.ceilingHeight - adjacentSector.ceilingHeight;
                        float floorDiff = adjacentSector.floorHeight - sector.floorHeight;
                        
                        // Set flag if there's a significant height difference
                        if (std::abs(ceilingDiff) > 0.05f || std::abs(floorDiff) > 0.05f) {
                            slice.hasHeightDifference = true;
                            
                            // Set texture coordinate ranges for upper and lower sections
                            slice.upperTexCoordV = 0.0f;  // Top of texture
                            slice.lowerTexCoordV = 0.5f;  // Middle of texture
                        }
                        
                        adjacentSectorHeight = adjacentSector.ceilingHeight;
                        
                        // Check if the height difference is very small (potential z-fighting cause)
                        float heightDiff = std::abs(sector.ceilingHeight - adjacentSector.ceilingHeight);
                        float floorHeightDiff = std::abs(sector.floorHeight - adjacentSector.floorHeight);
                        
                        // Flag portals with nearly equal heights or narrow portals
                        if ((heightDiff < 0.05f && heightDiff > 0.001f) || 
                            (floorHeightDiff < 0.05f && floorHeightDiff > 0.001f) || 
                            wallLength < 0.5f) {
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
    // Calculate base height and screen position
    const float viewHeight = view.height;
    
    // Get texture for this wall
    const Texture& texture = m_textures[slice.textureId % m_textures.size()];
    
    // Calculate screen Y coordinates for wall top and bottom
    int screenY1, screenY2;
    
    if (slice.isPortal && slice.hasHeightDifference) {
        // For portals with height differences, three sections may need rendering
        
        // Upper section (if needed)
        if (slice.adjacentCeilingHeight < slice.ceilingHeight) {
            float upperHeight = slice.ceilingHeight - slice.adjacentCeilingHeight;
            float projectedUpperHeight = calculateWallHeight(slice.distance, upperHeight);
            
            // Calculate screen Y coordinates
            float yCenter = m_height / 2.0f;
            float ceilingBaseY = yCenter - calculateWallHeight(slice.distance, slice.ceilingHeight - viewHeight);
            float adjacentCeilingY = yCenter - calculateWallHeight(slice.distance, slice.adjacentCeilingHeight - viewHeight);
            
            screenY1 = static_cast<int>(ceilingBaseY);
            screenY2 = static_cast<int>(adjacentCeilingY);
            
            renderTexturedWallStrip(slice, screenY1, screenY2, texture, slice.upperTexCoordV, 0.0f);
        }
        
        // Lower section (if needed)
        if (slice.adjacentFloorHeight > slice.floorHeight) {
            float lowerHeight = slice.adjacentFloorHeight - slice.floorHeight;
            float projectedLowerHeight = calculateWallHeight(slice.distance, lowerHeight);
            
            // Calculate screen Y coordinates
            float yCenter = m_height / 2.0f;
            float floorBaseY = yCenter + calculateWallHeight(slice.distance, viewHeight - slice.floorHeight);
            float adjacentFloorY = yCenter + calculateWallHeight(slice.distance, viewHeight - slice.adjacentFloorHeight);
            
            screenY1 = static_cast<int>(adjacentFloorY);
            screenY2 = static_cast<int>(floorBaseY);
            
            renderTexturedWallStrip(slice, screenY1, screenY2, texture, slice.lowerTexCoordV, 0.5f);
        }
    }
    else {
        // Calculate standard wall heights
        float wallHeight = slice.ceilingHeight - slice.floorHeight;
        float projectedHeight = calculateWallHeight(slice.distance, wallHeight);
        
        // Calculate screen positions
        int screenCenterY = m_height / 2;
        float eyeHeight = viewHeight - slice.floorHeight;
        float topOffset = (wallHeight - eyeHeight) / wallHeight * projectedHeight;
        float bottomOffset = eyeHeight / wallHeight * projectedHeight;
        
        screenY1 = screenCenterY - static_cast<int>(topOffset);
        screenY2 = screenCenterY + static_cast<int>(bottomOffset);
        
        // Clamp to screen
        screenY1 = std::max(0, screenY1);
        screenY2 = std::min(m_height - 1, screenY2);
        
        // Store wall extents for use in floor/ceiling rendering
        if (screenY1 < m_wallExtents[slice.x].top || m_wallExtents[slice.x].top == 0) {
            m_wallExtents[slice.x].top = screenY1;
        }
        if (screenY2 > m_wallExtents[slice.x].bottom) {
            m_wallExtents[slice.x].bottom = screenY2;
        }
        
        // Render textured wall column
        renderTexturedWallStrip(slice, screenY1, screenY2, texture);
    }
}

// Helper function to render a textured wall strip
void Renderer::renderTexturedWallStrip(
    const WallSlice& slice, int y1, int y2, const Texture& texture, 
    float texVOffset, float texVScale) {
    
    // Skip if off screen
    if (y2 < y1 || y1 >= m_height || y2 < 0 || slice.x < 0 || slice.x >= m_width) {
        return;
    }
    
    // Clamp to screen
    y1 = std::max(0, y1);
    y2 = std::min(m_height - 1, y2);
    
    // Texture mapping
    const int wallHeight = y2 - y1 + 1;
    const float texU = slice.texCoordU;
    const int texWidth = texture.width();
    
    // Precalculate shading factors for better performance
    float lightLevel = slice.lightLevel / 255.0f;
    
    // Apply stronger distance shading for better depth perception
    // This will make distant walls darker, improving depth cues
    float distanceFactor = 1.0f - slice.distance / m_skybox.maxViewDistance;
    distanceFactor = std::max(0.3f, distanceFactor); // Increased base visibility (was 0.2f)
    
    // Enhance the contrast to make walls more visible
    float shadingFactor = lightLevel * distanceFactor;
    shadingFactor = shadingFactor * shadingFactor * 2.0f; // Increased contrast enhancement (was 1.5f)
    shadingFactor = std::min(1.5f, shadingFactor); // Allow more brightness (was 1.2f)
    
    // Add a stronger brightening effect for edges to improve wall visibility
    float edgeBrightness = 0.0f;
    if (texU < 0.08f || texU > 0.92f) {
        // Brighten the edges of walls significantly
        edgeBrightness = 0.25f; // Increased from 0.15f
    }
    
    // Draw borders at the top and bottom of the wall for better visibility
    const int borderThickness = 2;
    
    for (int y = y1; y <= y2; y++) {
        // Calculate texture V coordinate
        float texV = (y - y1) / static_cast<float>(wallHeight);
        texV = texV * texVScale + texVOffset;
        
        // Get texture pixel color
        Color texColor = texture.sample(texU, texV);
        
        // Check if we're at a border (top or bottom of wall)
        bool isBorder = (y <= y1 + borderThickness) || (y >= y2 - borderThickness);
        
        // Apply shading based on light level and distance
        Color finalColor;
        
        if (isBorder) {
            // Draw borders in bright green to make walls stand out
            finalColor = Color(0, 255, 0);
        } else {
            // Normal wall shading with enhanced brightness and green tint
            finalColor = Color(
                static_cast<uint8_t>(std::min(255.0f, texColor.r * (shadingFactor + edgeBrightness) * 0.5f)),  // Reduce red component
                static_cast<uint8_t>(std::min(255.0f, texColor.g * (shadingFactor + edgeBrightness) * 1.5f)),  // Enhance green component
                static_cast<uint8_t>(std::min(255.0f, texColor.b * (shadingFactor + edgeBrightness) * 0.5f))   // Reduce blue component
            );
        }
        
        // Store depth in z-buffer and draw pixel
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
        
        // Calculate the world z at this scanline with improved horizon handling
        
        // Distance from horizon in pixels
        float pixelDistFromHorizon = static_cast<float>(y - horizonY);
        
        // Calculate a safe offset that never approaches zero
        // Use a smooth transition near the horizon to prevent visual artifacts
        float yOffset;
        
        // Define a "safe zone" around the horizon
        const float HORIZON_SAFE_ZONE = 4.0f;
        
        if (std::abs(pixelDistFromHorizon) < HORIZON_SAFE_ZONE) {
            // For pixels very close to the horizon, use a smoothly interpolated offset
            // that never gets too close to zero
            float t = pixelDistFromHorizon / HORIZON_SAFE_ZONE; // -1 to 1 range
            
            // Apply sigmoid-like function to create smooth transition at horizon
            // This ensures the offset never gets too close to zero
            float sign = (pixelDistFromHorizon < 0.0f) ? -1.0f : 1.0f;
            yOffset = sign * (HORIZON_SAFE_ZONE * 0.5f * (1.0f + std::abs(t)));
        } else {
            // For pixels far from the horizon, use the actual pixel offset
            yOffset = pixelDistFromHorizon;
        }
        
        // Ensure we never divide by something too close to zero
        if (std::abs(yOffset) < 0.1f) {
            yOffset = (yOffset < 0.0f) ? -0.1f : 0.1f;
        }
        
        // Improved distance calculation to prevent extreme values
        float heightDifference = view.height - visplane.height;
        float z;
        
        // Determine if we're looking up or down based on height difference and floor/ceiling
        bool lookingDown = (visplane.isFloor && heightDifference > 0.0f) || 
                           (!visplane.isFloor && heightDifference < 0.0f);
        
        // Calculate z with stabilized offset
        if (lookingDown) {
            z = DISTANCE_MULTIPLIER * std::abs(heightDifference) / std::abs(yOffset);
        } else {
            z = DISTANCE_MULTIPLIER * std::abs(heightDifference) / std::abs(yOffset);
        }
        
        // Apply the sign adjustment based on floor/ceiling
        if (visplane.isFloor) {
            z = (yOffset > 0.0f) ? z : -z;
        } else {
            z = (yOffset < 0.0f) ? z : -z;
        }
        
        // Clamp z to a reasonable range to prevent rendering artifacts
        z = std::max(0.1f, std::min(z, 100.0f));
        
        // Apply different depth biases based on plane type
        float depthBias = visplane.isFloor ? 0.05f : -0.05f;
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
    
    // Apply more sophisticated depth adjustment to prevent z-fighting
    // Calculate depth bias based on the distance and whether this is a floor or ceiling
    float distanceBasedBias = std::min(0.15f, std::abs(z) * 0.01f);
    
    // Apply different biases for floor and ceiling to ensure proper depth order
    float depthBias;
    if (span.isFloor) {
        // For floors, bias slightly behind based on distance
        depthBias = distanceBasedBias;
    } else {
        // For ceilings, bias slightly in front based on distance
        depthBias = -distanceBasedBias;
    }
    
    // Add a screen-position based component to ensure consistent depth ordering
    // This helps with surfaces at the same world position but different screen positions
    float screenPosFactor = static_cast<float>(span.y) / m_height;
    float screenBias = 0.0f;
    
    // Only apply screen bias for nearby surfaces (avoid impacting distant rendering)
    if (std::abs(z) < 10.0f) {
        if (span.isFloor) {
            // For floors, lower on screen = further away
            screenBias = screenPosFactor * 0.05f;
        } else {
            // For ceilings, higher on screen = further away
            screenBias = (1.0f - screenPosFactor) * 0.05f;
        }
    }
    
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
        float fogFactor = 1.0f - std::min(1.0f, std::abs(z) / 30.0f);
        finalColor = Color::blend(Color(0, 0, 0), finalColor, fogFactor);
        
        // Apply combined depth bias
        float adjustedZ = z + depthBias + screenBias;
        
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
            // Track sprite type to improve sorting
            sortedSprites.emplace_back(&sprite, distance);
        }
    }
    
    // Sort sprites with improved ordering:
    // 1. First by transparency - opaque sprites are rendered first
    // 2. Then by distance - sorted back to front
    // 3. Finally by type - items get priority for similar distances
    std::sort(sortedSprites.begin(), sortedSprites.end(), 
        [this](const SpriteRenderData& a, const SpriteRenderData& b) {
            // Check for sprite transparency - get frame and check alpha at center
            bool aTransparent = false;
            bool bTransparent = false;
            
            // Check sprite types and transparency as a sort key
            const SpriteFrame& aFrame = a.sprite->getCurrentFrame();
            const SpriteFrame& bFrame = b.sprite->getCurrentFrame();
            
            if (aFrame.textureId >= 0 && aFrame.textureId < static_cast<int>(m_textures.size())) {
                Color aColor = m_textures[aFrame.textureId].sample(0.5f, 0.5f);
                aTransparent = (aColor.a < 240);
            }
            
            if (bFrame.textureId >= 0 && bFrame.textureId < static_cast<int>(m_textures.size())) {
                Color bColor = m_textures[bFrame.textureId].sample(0.5f, 0.5f);
                bTransparent = (bColor.a < 240);
            }
            
            // First sort by transparency (opaque sprites first)
            if (aTransparent != bTransparent) {
                return !aTransparent; // Opaque sprites first (!aTransparent is true when a is opaque)
            }
            
            // For opaque sprites, sort back to front
            if (!aTransparent) {
                return a.distance > b.distance;
            }
            
            // For transparent sprites, sort type first (items appear in front)
            bool aIsItem = (a.sprite->type == SpriteType::ITEM);
            bool bIsItem = (b.sprite->type == SpriteType::ITEM);
            
            if (aIsItem != bIsItem) {
                return aIsItem; // Items first
            }
            
            // Then for transparent sprites of same type, sort back to front
            return a.distance > b.distance;
        });
    
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
    
    // Calculate projected sprite height using the consistent projection function
    float projectedHeight = calculateWallHeight(distance, spriteHeight);
    
    // Calculate vertical position - use consistent positioning method
    float spriteTopWorld = spriteY + spriteHeight;
    float spriteBottomWorld = spriteY;
    
    // Use the consistent function to calculate screen positions
    int spriteTop = static_cast<int>(calculateScreenYPosition(spriteTopWorld, distance, view.height));
    int spriteBottom = static_cast<int>(calculateScreenYPosition(spriteBottomWorld, distance, view.height));
    
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
    
    // Calculate sprite depth bias - smaller bias for non-transparent sprites, larger for transparent
    // This helps prevent z-fighting with walls while maintaining proper sprite-to-sprite ordering
    bool hasTransparency = false;
    float depthBias = 0.005f; // Base bias for all sprites
    
    // Determine if sprite has transparent pixels (simple check by sampling a few points)
    // Only do this for sprites with potential transparency
    if (sprite.type == SpriteType::ITEM || sprite.type == SpriteType::ENEMY) {
        // Sample center pixel
        Color centerColor = texture.sample(0.5f, 0.5f);
        if (centerColor.a < 240) {
            hasTransparency = true;
            depthBias = 0.01f; // Larger bias for transparent sprites
        }
    }
    
    // Check if this sprite is an item or pickup - give those priority in depth testing
    if (sprite.type == SpriteType::ITEM) {
        depthBias = -0.01f; // Negative bias brings items slightly forward
    }
    
    // Draw the sprite with improved column traversal for better performance
    // First, pre-calculate texture coordinates for each column
    std::vector<float> uCoords(right - left + 1);
    
    for (int i = 0; i <= right - left; i++) {
        int x = left + i;
        float screenX = static_cast<float>(x - spriteLeft) / (spriteRight - spriteLeft);
        if (sprite.flipped) {
            screenX = 1.0f - screenX;
        }
        uCoords[i] = screenX;
    }
    
    // Now draw column by column (better cache coherence)
    for (int i = 0; i <= right - left; i++) {
        int x = left + i;
        float u = uCoords[i];
        
        // Calculate depth with subtle variation based on distance from center
        // This creates a more 3D feel for sprites
        float distFromCenter = std::abs(u - 0.5f) * 2.0f; // 0 at center, 1 at edges
        float columnDistance = distance * (1.0f + distFromCenter * 0.1f);
        float adjustedDepth = columnDistance + depthBias;
        
        // First check if this column is fully occluded by walls
        bool columnVisible = false;
        for (int y = top; y <= bottom; y++) {
            if (adjustedDepth < getDepth(x, y)) {
                columnVisible = true;
                break;
            }
        }
        
        // Skip column if fully occluded
        if (!columnVisible) continue;
        
        // Draw vertical stripe top to bottom
        for (int y = top; y <= bottom; y++) {
            // Calculate texture v coordinate
            float screenY = static_cast<float>(y - spriteTop) / (spriteBottom - spriteTop);
            float v = screenY;
            
            // Calculate precise depth for this pixel with variable bias based on v position
            // This helps create a subtle "curved" effect
            float pixelDepth = adjustedDepth;
            
            // Sample texture
            Color color = texture.sample(u, v);
            
            // Skip fully transparent pixels
            if (color.a < 10) {
                continue;
            }
            
            // For semi-transparent pixels, blend with background instead of overwriting
            if (color.a < 240 && hasTransparency) {
                // Get current color at this pixel
                Color bgColor = m_frameBuffer[y * m_width + x];
                
                // Blend based on alpha
                float alpha = color.a / 255.0f;
                Color litColor = Color::blend(Color(0, 0, 0), color, lightFactor);
                
                // Apply fog
                Color spriteColor = Color::blend(Color(0, 0, 0), litColor, fogFactor);
                
                // Final blended color
                Color finalColor(
                    static_cast<uint8_t>(bgColor.r * (1.0f - alpha) + spriteColor.r * alpha),
                    static_cast<uint8_t>(bgColor.g * (1.0f - alpha) + spriteColor.g * alpha),
                    static_cast<uint8_t>(bgColor.b * (1.0f - alpha) + spriteColor.b * alpha)
                );
                
                // Only draw if in front of the wall
                if (pixelDepth < getDepth(x, y)) {
                    m_frameBuffer[y * m_width + x] = finalColor;
                    // Don't update z-buffer for semi-transparent pixels to allow
                    // other sprites behind this one to still be visible
                }
            } 
            else {
                // Fully opaque or nearly opaque pixel
                // Apply lighting
                Color litColor = Color::blend(Color(0, 0, 0), color, lightFactor);
                
                // Apply fog
                Color finalColor = Color::blend(Color(0, 0, 0), litColor, fogFactor);
                
                // Draw with standard depth checking
                drawPixelWithDepth(x, y, pixelDepth, finalColor);
            }
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
        // If fully opaque, just set the color directly
        if (color.a == 255) {
            m_frameBuffer[y * m_width + x] = color;
            return;
        }
        
        // Handle alpha blending
        Color& destColor = m_frameBuffer[y * m_width + x];
        float alpha = color.a / 255.0f;
        float invAlpha = 1.0f - alpha;
        
        // Blend the colors based on alpha
        destColor.r = static_cast<uint8_t>(color.r * alpha + destColor.r * invAlpha);
        destColor.g = static_cast<uint8_t>(color.g * alpha + destColor.g * invAlpha);
        destColor.b = static_cast<uint8_t>(color.b * alpha + destColor.b * invAlpha);
        destColor.a = 255; // Result is fully opaque
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
    
    // Apply a stronger height multiplier to ensure walls are visible
    return (DISTANCE_MULTIPLIER * wallHeight) / distance;
}

// Consistent helper function to calculate screen Y position based on world height and distance
float Renderer::calculateScreenYPosition(float worldY, float distance, float viewHeight) const {
    if (distance < 0.1f) distance = 0.1f; // Prevent division by zero
    
    // Calculate height relative to player eye level
    float relativeHeight = worldY - viewHeight;
    
    // Get screen center
    int centerY = m_height / 2;
    
    // Scale the height based on distance using perspective projection formula
    // This ensures correct perspective even for close/far objects
    float scaledHeight = (relativeHeight * DISTANCE_MULTIPLIER) / distance;
    
    // Return screen Y coordinate (top of screen is 0)
    return centerY - scaledHeight;
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
    // Get current z-buffer depth
    float currentDepth = getDepth(x, y);
    
    // Improved depth comparison with adaptive bias to prevent z-fighting
    
    // Use different bias values based on depth range for better precision
    // Closer objects need smaller bias values for detail preservation
    // Distant objects can use larger bias to avoid z-fighting
    float DEPTH_BIAS;
    
    if (std::abs(depth) < 1.0f) {
        // Very close objects - use minimal bias
        DEPTH_BIAS = 0.0001f;
    } else if (std::abs(depth) < 5.0f) {
        // Medium distance objects
        DEPTH_BIAS = 0.001f;
    } else if (std::abs(depth) < 20.0f) {
        // Far objects
        DEPTH_BIAS = 0.01f;
    } else {
        // Very distant objects
        DEPTH_BIAS = 0.05f;
    }
    
    // For very close surfaces (potential z-fighting)
    if (std::abs(depth - currentDepth) < 0.05f) {
        // Add screen-space coherence factor - stabilize across adjacent pixels
        // This avoids flickering when depth values are very close
        if (x % 2 == 0 && y % 2 == 0) {
            // Use a stable tie-breaking rule to ensure consistent results
            // Choose based on depth and a spatial pattern for stability
            return depth < currentDepth;
        } else {
            // Compare with bias for other pixels
            return depth < (currentDepth - DEPTH_BIAS);
        }
    }
    
    // For normal cases with clearly different depths
    return depth < currentDepth;
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
    // Skip if GPU acceleration is enabled - it's handled in renderFrame
    if (isGpuAccelerationEnabled()) return;
    
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
                setDepth(x + i, y, 1.0f);
                if (performanceMode && y + 1 < horizonY) {
                    setDepth(x + i, y + 1, 1.0f);
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
    // Skip if GPU acceleration is enabled - it's handled in renderSkyboxCuda
    if (isGpuAccelerationEnabled()) return;
    
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

// Implementation of minimap rendering
Vec2 Renderer::worldToMinimap(const Vec2& worldPos) const {
    // Convert from world coordinates to minimap coordinates
    float minimapCenterX = m_minimapX + m_minimapSize / 2.0f;
    float minimapCenterY = m_minimapY + m_minimapSize / 2.0f;
    
    // Scale and translate the world position to minimap position
    // Center the map on the player's position rather than a hardcoded point
    float scale = m_minimapScale * 0.4f; // Scale factor to show more of the map
    
    // Use the player's current position (stored during renderMinimap call)
    // This makes the minimap follow the player, showing relevant surroundings
    float minimapX = minimapCenterX + (worldPos.x - m_playerPos.x) * scale;
    float minimapY = minimapCenterY + (worldPos.y - m_playerPos.y) * scale;
    
    return Vec2(minimapX, minimapY);
}

void Renderer::drawMinimapWall(int x1, int y1, int x2, int y2, const Color& color) {
    // Bresenham's line algorithm for drawing walls on the minimap
    int dx = std::abs(x2 - x1);
    int dy = std::abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        // Draw the point if it's inside the minimap area
        if (x1 >= m_minimapX && x1 < m_minimapX + m_minimapSize &&
            y1 >= m_minimapY && y1 < m_minimapY + m_minimapSize) {
            drawPixel(x1, y1, color);
        }
        
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void Renderer::drawMinimapPlayer(int x, int y, float angle, const Color& color) {
    // Draw player position as a circle
    int radius = 3;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx*dx + dy*dy <= radius*radius) {
                int drawX = x + dx;
                int drawY = y + dy;
                if (drawX >= m_minimapX && drawX < m_minimapX + m_minimapSize &&
                    drawY >= m_minimapY && drawY < m_minimapY + m_minimapSize) {
                    drawPixel(drawX, drawY, color);
                }
            }
        }
    }
    
    // Draw player direction as a line
    int dirX = x + static_cast<int>(std::cos(angle) * radius * 2);
    int dirY = y + static_cast<int>(std::sin(angle) * radius * 2);
    drawMinimapWall(x, y, dirX, dirY, Color(255, 255, 255)); // White direction line
}

void Renderer::renderMinimap(const BSPTree& bsp, const ViewPosition& view) {
    // Store player position for minimap centering
    m_playerPos = view.position;
    
    // Draw minimap background
    for (int y = m_minimapY; y < m_minimapY + m_minimapSize; y++) {
        for (int x = m_minimapX; x < m_minimapX + m_minimapSize; x++) {
            drawPixel(x, y, Color(0, 0, 0, 200)); // More opaque background
        }
    }
    
    // Draw minimap border
    for (int x = m_minimapX; x < m_minimapX + m_minimapSize; x++) {
        drawPixel(x, m_minimapY, Color(255, 255, 255));
        drawPixel(x, m_minimapY + m_minimapSize - 1, Color(255, 255, 255));
    }
    for (int y = m_minimapY; y < m_minimapY + m_minimapSize; y++) {
        drawPixel(m_minimapX, y, Color(255, 255, 255));
        drawPixel(m_minimapX + m_minimapSize - 1, y, Color(255, 255, 255));
    }
    
    // Draw all walls from all sectors
    const std::vector<Sector>& sectors = bsp.getSectors();
    
    // Debug: Count total walls
    int totalWalls = 0;
    for (const auto& sector : sectors) {
        totalWalls += sector.walls.size();
    }
    
    // Draw a text indicator in the top-left corner of the minimap
    std::string wallCountStr = "Walls: " + std::to_string(totalWalls);
    int textX = m_minimapX + 5;
    int textY = m_minimapY + 5;
    
    // Draw each letter as a small rectangle
    for (size_t i = 0; i < wallCountStr.length(); i++) {
        // Simple 3x5 pixel character (just a placeholder)
        for (int dy = 0; dy < 5; dy++) {
            for (int dx = 0; dx < 3; dx++) {
                drawPixel(textX + i*4 + dx, textY + dy, Color(255, 255, 255));
            }
        }
    }
    
    // Set an appropriate scale to show the entire map clearly
    float originalScale = m_minimapScale;
    m_minimapScale = 0.7f; // Smaller value = more zoomed out (showing more of the map)
    
    // Draw all sectors in bright colors
    for (size_t i = 0; i < sectors.size(); i++) {
        const Sector& sector = sectors[i];
        
        for (const Wall& wall : sector.walls) {
            // Convert world coordinates to minimap coordinates
            Vec2 start = worldToMinimap(wall.segment.start.position);
            Vec2 end = worldToMinimap(wall.segment.end.position);
            
            // Choose color based on wall type (solid walls vs portals)
            Color wallColor;
            if (wall.sectorBack == -1) {
                wallColor = Color(255, 80, 80, 255); // Bright red for solid walls
            } else {
                wallColor = Color(80, 255, 80, 255); // Bright green for portals
            }
            
            // Make the walls thicker for better visibility
            // Draw the main line
            drawMinimapWall(
                static_cast<int>(start.x), static_cast<int>(start.y),
                static_cast<int>(end.x), static_cast<int>(end.y),
                wallColor
            );
            
            // Draw additional lines for thickness - increased to 5 pixels thick
            for (int offset = 1; offset <= 5; offset++) {
                // Horizontal thickening
                drawMinimapWall(
                    static_cast<int>(start.x), static_cast<int>(start.y) - offset,
                    static_cast<int>(end.x), static_cast<int>(end.y) - offset,
                    wallColor
                );
                drawMinimapWall(
                    static_cast<int>(start.x), static_cast<int>(start.y) + offset,
                    static_cast<int>(end.x), static_cast<int>(end.y) + offset,
                    wallColor
                );
                
                // Vertical thickening
                drawMinimapWall(
                    static_cast<int>(start.x) - offset, static_cast<int>(start.y),
                    static_cast<int>(end.x) - offset, static_cast<int>(end.y),
                    wallColor
                );
                drawMinimapWall(
                    static_cast<int>(start.x) + offset, static_cast<int>(start.y),
                    static_cast<int>(end.x) + offset, static_cast<int>(end.y),
                    wallColor
                );
            }
            
            // Draw endpoints as circles for better visibility
            int pointRadius = 3;
            
            // Start point circle
            for (int dy = -pointRadius; dy <= pointRadius; dy++) {
                for (int dx = -pointRadius; dx <= pointRadius; dx++) {
                    if (dx*dx + dy*dy <= pointRadius*pointRadius) {
                        drawPixel(
                            static_cast<int>(start.x) + dx, 
                            static_cast<int>(start.y) + dy, 
                            Color(255, 255, 0) // Yellow for endpoints
                        );
                    }
                }
            }
            
            // End point circle
            for (int dy = -pointRadius; dy <= pointRadius; dy++) {
                for (int dx = -pointRadius; dx <= pointRadius; dx++) {
                    if (dx*dx + dy*dy <= pointRadius*pointRadius) {
                        drawPixel(
                            static_cast<int>(end.x) + dx, 
                            static_cast<int>(end.y) + dy, 
                            Color(255, 255, 0) // Yellow for endpoints
                        );
                    }
                }
            }
        }
    }
    
    // Debug visualization: Show collision points if enabled
    if (m_settings.showCollisions) {
        // Check for potential collisions in multiple directions
        const float radius = 0.35f;  // Match player radius from main.cpp
        const int numRays = 32;     // Cast more rays for better coverage
        
        for (int i = 0; i < numRays; i++) {
            float angle = (float)i * 2.0f * PI / numRays;
            Vec2 dir(std::cos(angle), std::sin(angle));
            
            // Cast a ray in this direction to find potential collisions
            CollisionInfo info = bsp.castRay(view.position, dir, radius * 3.0f);
            
            if (info.collision) {
                // Convert collision point to minimap coordinates
                Vec2 collPoint = worldToMinimap(info.point);
                int collX = static_cast<int>(collPoint.x);
                int collY = static_cast<int>(collPoint.y);
                
                // Draw collision point in bright magenta for visibility
                for (int dy = -3; dy <= 3; dy++) {
                    for (int dx = -3; dx <= 3; dx++) {
                        if (dx*dx + dy*dy <= 9) {
                            drawPixel(collX + dx, collY + dy, Color(255, 0, 255, 255));
                        }
                    }
                }
                
                // Draw wall normal at collision point
                Vec2 normalEnd = worldToMinimap(info.point + info.normal * 1.0f);
                drawMinimapWall(
                    collX, collY,
                    static_cast<int>(normalEnd.x), static_cast<int>(normalEnd.y),
                    Color(255, 255, 0, 255)
                );
            }
        }
    }
    
    // Restore original scale
    m_minimapScale = originalScale;
    
    // Draw player position and direction - should be centered on the minimap
    Vec2 playerPos = worldToMinimap(view.position);
    // The player should always be at the center now
    drawMinimapPlayer(
        static_cast<int>(playerPos.x),
        static_cast<int>(playerPos.y),
        view.angle,
        Color(255, 255, 255) // White for better visibility
    );
    
    // Draw grid lines (every 10 units)
    Color gridColor(100, 100, 100, 64); // More transparent grid
    for (int grid = -100; grid <= 100; grid += 10) {
        // Vertical grid line
        Vec2 gridStart = worldToMinimap(Vec2(grid, -100));
        Vec2 gridEnd = worldToMinimap(Vec2(grid, 100));
        drawMinimapWall(
            static_cast<int>(gridStart.x), static_cast<int>(gridStart.y),
            static_cast<int>(gridEnd.x), static_cast<int>(gridEnd.y),
            gridColor
        );
        
        // Horizontal grid line
        gridStart = worldToMinimap(Vec2(-100, grid));
        gridEnd = worldToMinimap(Vec2(100, grid));
        drawMinimapWall(
            static_cast<int>(gridStart.x), static_cast<int>(gridStart.y),
            static_cast<int>(gridEnd.x), static_cast<int>(gridEnd.y),
            gridColor
        );
    }
    
    // Print current player coordinates at the bottom of minimap
    std::string posStr = "Pos: (" + std::to_string(int(view.position.x)) + "," + 
                       std::to_string(int(view.position.y)) + ")";
    int posTextX = m_minimapX + 5;
    int posTextY = m_minimapY + m_minimapSize - 10;
    
    // Draw each letter as a small rectangle
    for (size_t i = 0; i < posStr.length(); i++) {
        // Simple 3x5 pixel character
        for (int dy = 0; dy < 5; dy++) {
            for (int dx = 0; dx < 3; dx++) {
                drawPixel(posTextX + i*4 + dx, posTextY + dy, Color(255, 255, 0)); // Yellow text
            }
        }
    }
}

// Also update the setGpuAccelerationEnabled method to handle BSP data
void Renderer::setGpuAccelerationEnabled(bool enabled) {
    // Only change if there's actually a change
    if (m_gpuAccelerationEnabled != enabled) {
        m_gpuAccelerationEnabled = enabled;
        
        // If turning off GPU, make sure we'll reupload next time it's enabled
        if (!enabled) {
            // Reset flags to ensure data is re-uploaded when GPU is re-enabled
            if (m_cudaRenderer) {
                // Free GPU resources since we won't be using them
                m_cudaRenderer->freeBSPData();
            }
        }
    }
}

void Renderer::addTexture(const Texture& texture) {
    // Add to texture list
    m_textures.push_back(texture);
    
    // Force texture re-upload flag regardless of CUDA status
    m_texturesUploaded = false;
    
    // Log the addition
    std::cout << "Added new texture to renderer, total textures: " << m_textures.size() << std::endl;
    
    // If GPU acceleration is enabled, upload the updated texture list
    #if defined(ENABLE_CUDA)
    if (m_gpuAccelerationEnabled && m_cudaRenderer) {
        try {
            std::cout << "Uploading added texture to GPU, total textures: " << m_textures.size() << std::endl;
            m_cudaRenderer->uploadTextures(m_textures);
            
            // Note: We don't set m_texturesUploaded to true here because we want the main
            // render loop to verify the upload was successful during the next frame
        } catch (const std::exception& e) {
            std::cerr << "Error uploading new texture to GPU: " << e.what() << std::endl;
            // Continue using CPU rendering for this texture
        }
    }
    #endif
}

// Render platforms
void Renderer::renderPlatforms(const BSPTree& bsp, const ViewPosition& view) {
    // Get all platforms from the BSP tree
    const std::vector<Platform>& platforms = bsp.getPlatforms();
    
    // Render each platform
    for (const Platform& platform : platforms) {
        // Check if the platform is in the current sector or a visible sector
        int sectorId = platform.sectorId;
        if (sectorId >= 0 && bsp.isSectorVisible(sectorId, view.position, view.angle, view.fov)) {
            renderPlatform(platform, view);
        }
    }
}

// Render a single platform
void Renderer::renderPlatform(const Platform& platform, const ViewPosition& view) {
    // Render the top surface
    renderPlatformSurface(platform, true, view);
    
    // Render the bottom surface
    renderPlatformSurface(platform, false, view);
    
    // Render the sides
    renderPlatformSides(platform, view);
}

// Helper method to render a platform surface (top or bottom)
void Renderer::renderPlatformSurface(const Platform& platform, bool isTop, const ViewPosition& view) {
    // Get the height of the surface
    float surfaceHeight = isTop ? platform.getTopHeight() : platform.getBottomHeight();
    
    // Get the texture ID for the surface
    int textureId = isTop ? platform.topTextureId : platform.bottomTextureId;
    
    // Skip if no texture is assigned
    if (textureId < 0 || textureId >= static_cast<int>(m_textures.size())) {
        return;
    }
    
    // Get the texture
    const Texture& texture = m_textures[textureId];
    
    // Calculate the height difference between the view and the surface
    float heightDiff = view.height - surfaceHeight;
    
    // Skip rendering if the surface is at the same height as the view
    if (std::abs(heightDiff) < 0.001f) {
        return;
    }
    
    // Determine if we're looking at the surface from above or below
    bool lookingFromAbove = heightDiff > 0;
    
    // Skip rendering the bottom of the platform if we're looking from below
    if (!isTop && lookingFromAbove) {
        return;
    }
    
    // Skip rendering the top of the platform if we're looking from above
    if (isTop && !lookingFromAbove) {
        return;
    }
    
    // Create a visplane for the surface
    Visplane visplane;
    visplane.height = surfaceHeight;
    visplane.textureId = textureId;
    visplane.lightLevel = platform.lightLevel;
    visplane.isFloor = isTop; // Top surface is like a floor, bottom is like a ceiling
    
    // Resize the columns vector to match the screen width
    visplane.columns.resize(m_width);
    
    // Calculate the screen space coordinates for each vertex of the platform
    std::vector<Vec2> screenVertices;
    for (const Vec2& vertex : platform.vertices) {
        // Convert world space to screen space
        screenVertices.push_back(worldToScreen(vertex, view));
    }
    
    // Fill the visplane columns based on the platform's screen space polygon
    // This is a simplified approach - in a real implementation, you would need to handle
    // clipping, occlusion, and other edge cases
    
    // For each screen column
    for (int x = 0; x < m_width; ++x) {
        // Check if this column intersects with the platform's screen space polygon
        // This is a simple ray casting algorithm
        int intersections = 0;
        for (size_t i = 0; i < screenVertices.size(); ++i) {
            size_t j = (i + 1) % screenVertices.size();
            
            // Check if the edge crosses this column
            if ((screenVertices[i].x <= x && screenVertices[j].x > x) ||
                (screenVertices[j].x <= x && screenVertices[i].x > x)) {
                
                // Calculate the y-coordinate of the intersection
                float t = (x - screenVertices[i].x) / (screenVertices[j].x - screenVertices[i].x);
                float y = screenVertices[i].y + t * (screenVertices[j].y - screenVertices[i].y);
                
                // Count the intersection
                intersections++;
                
                // Update the visplane column
                if (intersections % 2 == 1) {
                    visplane.columns[x].yStart = static_cast<int>(y);
                } else {
                    visplane.columns[x].yEnd = static_cast<int>(y);
                }
            }
        }
    }
    
    // Render the visplane
    renderVisplane(visplane, view);
}

// Helper method to render platform sides
void Renderer::renderPlatformSides(const Platform& platform, const ViewPosition& view) {
    // Get the texture ID for the sides
    int textureId = platform.sideTextureId;
    
    // Skip if no texture is assigned
    if (textureId < 0 || textureId >= static_cast<int>(m_textures.size())) {
        return;
    }
    
    // Get the texture
    const Texture& texture = m_textures[textureId];
    
    // Get the top and bottom heights
    float topHeight = platform.getTopHeight();
    float bottomHeight = platform.getBottomHeight();
    
    // For each edge of the platform
    for (size_t i = 0; i < platform.vertices.size(); ++i) {
        size_t j = (i + 1) % platform.vertices.size();
        
        // Create a wall slice for this edge
        WallSlice slice;
        slice.textureId = textureId;
        slice.lightLevel = platform.lightLevel;
        slice.floorHeight = bottomHeight;
        slice.ceilingHeight = topHeight;
        
        // Calculate the world space coordinates of the edge
        Vec2 start = platform.vertices[i];
        Vec2 end = platform.vertices[j];
        
        // Calculate the direction from the view to the edge
        Vec2 toStart = start - view.position;
        Vec2 toEnd = end - view.position;
        
        // Calculate the angle to the start and end points
        float angleToStart = std::atan2(toStart.y, toStart.x);
        float angleToEnd = std::atan2(toEnd.y, toEnd.x);
        
        // Normalize the angles to be within the view's field of view
        float viewAngle = view.angle;
        float halfFov = view.fov * 0.5f * DEG_TO_RAD;
        
        // Calculate the angle difference
        float angleDiff = angleToEnd - angleToStart;
        if (angleDiff > PI) angleDiff -= 2.0f * PI;
        if (angleDiff < -PI) angleDiff += 2.0f * PI;
        
        // Skip if the edge is not facing the view
        if (std::abs(angleDiff) < 0.001f) {
            continue;
        }
        
        // Calculate the normal of the edge
        Vec2 normal = platform.getEdgeNormal(i);
        
        // Skip if the edge is facing away from the view
        Vec2 toView = (view.position - start).normalized();
        if (normal.dotProduct(toView) <= 0.0f) {
            continue;
        }
        
        // Calculate the distance to the edge
        float distance = toStart.length();
        
        // Calculate the screen space x-coordinate
        float screenX = (angleToStart - (viewAngle - halfFov)) / (view.fov * DEG_TO_RAD) * m_width;
        
        // Calculate the wall height on screen
        float wallHeight = calculateWallHeight(distance, topHeight - bottomHeight);
        
        // Set up the wall slice
        slice.x = static_cast<int>(screenX);
        slice.distance = distance;
        slice.height = wallHeight;
        slice.texCoordU = 0.0f; // Start of the texture
        
        // Render the wall slice
        renderWallSlice(slice, view);
    }
}

} // namespace PureDoom 
