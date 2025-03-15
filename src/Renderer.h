#ifndef RENDERER_H
#define RENDERER_H

#include "BSPTree.h"
#include "Sprite.h"
#include <vector>
#include <memory>
#include <array>
#include <cstdint>
#include <string>

// Include CUDA runtime headers only when compiling with CUDA
#if defined(__CUDACC__) || defined(ENABLE_CUDA)
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#define CUDA_CALLABLE __host__ __device__
#else
#define CUDA_CALLABLE
// Define empty CUDA attributes for non-CUDA compilation
#define __host__
#define __device__
#endif

// Constant values
constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;
constexpr float DISTANCE_MULTIPLIER = 120.0f; // Used for projecting wall heights

namespace PureDoom {

// Forward declarations
struct Color;
struct ViewPosition;
struct RenderInfo;
struct WallSlice;
struct Span;
class Texture;
class RendererCuda; // Forward declaration for CUDA renderer

// Color representation (RGBA)
struct Color {
    uint8_t r, g, b, a;
    
    CUDA_CALLABLE Color() : r(0), g(0), b(0), a(255) {}
    CUDA_CALLABLE Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}
    
    // Make static methods CUDA-compatible with appropriate implementation for each context
    #if defined(__CUDACC__) || defined(ENABLE_CUDA)
    // For CUDA compilation, make these methods available on both host and device
    __host__ __device__ static Color fromHSV(float h, float s, float v);
    __host__ __device__ static Color blend(const Color& c1, const Color& c2, float t);
    #else
    // For regular C++ compilation
    static Color fromHSV(float h, float s, float v);
    static Color blend(const Color& c1, const Color& c2, float t);
    #endif
};

// View position and orientation
struct ViewPosition {
    Vec2 position;  // Player position in world space
    float angle;    // View angle in radians (0 = East, Counter-clockwise)
    float fov;      // Field of view in degrees
    float height;   // Camera height above floor
    
    ViewPosition() : angle(0.0f), fov(90.0f), height(0.8f) {}
    ViewPosition(const Vec2& pos, float ang, float fov = 90.0f, float h = 0.8f)
        : position(pos), angle(ang), fov(fov), height(h) {}
};

// Wall slice data for rendering
struct WallSlice {
    int x;            // Screen x coordinate
    float distance;   // Distance to the wall
    float height;     // Height on screen
    float texCoordU;  // Texture U coordinate (0-1)
    int textureId;    // Texture to use
    int lightLevel;   // Light level
    bool isPortal;    // Is this a portal wall?
    float floorHeight;   // Floor height of sector
    float ceilingHeight; // Ceiling height of sector
    bool isProblematicPortal; // Flag for portals that need special handling
    float adjacentSectorHeight; // Height of adjacent sector (for portals)
    
    WallSlice() : x(0), distance(0.0f), height(0.0f), texCoordU(0.0f),
                 textureId(-1), lightLevel(0), isPortal(false),
                 floorHeight(0.0f), ceilingHeight(0.0f),
                 isProblematicPortal(false), adjacentSectorHeight(0.0f) {}
};

// Span for floor/ceiling
struct Span {
    int y;           // Y coordinate of this span
    int startX;      // Start x coordinate
    int endX;        // End x coordinate
    float startU;    // Start u texture coordinate
    float startV;    // Start v texture coordinate
    float endU;      // End u texture coordinate
    float endV;      // End v texture coordinate
    float startZ;    // Start z depth
    float endZ;      // End z depth
    int textureId;   // Texture id
    int lightLevel;  // Light level
    bool isFloor;    // Is this a floor span (vs ceiling)?
    
    Span() : y(0), startX(0), endX(0), 
             startU(0.0f), startV(0.0f), endU(0.0f), endV(0.0f),
             startZ(0.0f), endZ(0.0f),
             textureId(-1), lightLevel(0), isFloor(true) {}
};

// Visplane column structure
struct VisplaneColumn {
    int yStart;   // Start y-coordinate for this column
    int yEnd;     // End y-coordinate for this column
    
    VisplaneColumn() : yStart(-1), yEnd(-1) {}
};

// Visplane for floor/ceiling
struct Visplane {
    std::vector<VisplaneColumn> columns; // Columns of this visplane
    float height;      // Height of this plane in world units
    int textureId;     // Texture id
    int lightLevel;    // Light level
    bool isFloor;      // Is this a floor plane (vs ceiling)?
    
    Visplane() : height(0.0f), textureId(0), lightLevel(255), isFloor(true) {}
};

// Simple texture class
class Texture {
public:
    Texture() : m_width(0), m_height(0) {}
    Texture(int width, int height);
    Texture(const std::string& filename);
    
    Color getPixel(int x, int y) const;
    Color sample(float u, float v) const;
    
    int width() const { return m_width; }
    int height() const { return m_height; }
    std::vector<Color> m_pixels;

private:
    int m_width, m_height;
    
    void loadFromFile(const std::string& filename);
    void generateCheckerboard();
};

// Skybox structure to represent a dynamic skybox
struct Skybox {
    Texture texture;            // Sky texture
    Color zenithColor;          // Color at the top of the sky
    Color horizonColor;         // Color at the horizon
    float sunSize;              // Size of the sun in degrees
    float sunAngle;             // Sun angle in radians (0 = East, counter-clockwise)
    float sunHeight;            // Sun height (-1 to 1, where 0 is at horizon)
    Color sunColor;             // Color of the sun
    Color sunGlowColor;         // Color of the sun's glow
    float sunGlowSize;          // Size of the sun's glow (multiplier of sun size)
    float timeOfDay;            // Time of day (0-1, where 0 = midnight, 0.5 = noon)
    bool dynamicSky;            // Whether to animate the sky
    float maxViewDistance;      // Maximum rendering distance for performance optimization
    
    Skybox() : 
        zenithColor(100, 150, 255),    // Blue sky at top
        horizonColor(220, 230, 255),   // Lighter blue at horizon
        sunSize(5.0f),                 // 5 degrees sun size
        sunAngle(0.0f),                // Starts at east
        sunHeight(0.2f),               // Just above horizon
        sunColor(255, 253, 230),       // Yellowish sun
        sunGlowColor(255, 200, 150),   // Orange-ish glow
        sunGlowSize(3.0f),             // 3x sun size for glow
        timeOfDay(0.3f),               // Morning
        dynamicSky(true),
        maxViewDistance(30.0f) {}      // Default max view distance
        
    // Update the sun position based on time
    void update(float deltaTime) {
        if (dynamicSky) {
            // Move time forward (completes a full cycle in 5 minutes of real time)
            timeOfDay += deltaTime / 300.0f;
            if (timeOfDay >= 1.0f) {
                timeOfDay -= 1.0f;
            }
            
            // Update sun position based on time of day
            sunAngle = timeOfDay * 2.0f * PI;
            
            // Sun height follows a sine curve (highest at noon, lowest at midnight)
            #if defined(__CUDACC__)
            sunHeight = sin((timeOfDay - 0.25f) * 2.0f * PI) * 0.8f;
            #else
            sunHeight = std::sin((timeOfDay - 0.25f) * 2.0f * PI) * 0.8f;
            #endif
            
            // Adjust colors based on time of day
            if (timeOfDay < 0.25f || timeOfDay > 0.75f) {
                // Night - darker sky
                float nightFactor = (timeOfDay < 0.25f) ? 
                    1.0f - (timeOfDay / 0.25f) : 
                    (timeOfDay - 0.75f) / 0.25f;
                
                zenithColor = Color::blend(
                    Color(100, 150, 255),  // Day blue
                    Color(10, 20, 80),     // Night blue
                    nightFactor
                );
                
                horizonColor = Color::blend(
                    Color(220, 230, 255),  // Day horizon
                    Color(50, 40, 100),    // Night horizon
                    nightFactor
                );
                
                // Adjust sun color for sunset/sunrise
                if (timeOfDay > 0.75f || timeOfDay < 0.05f || (timeOfDay > 0.45f && timeOfDay < 0.55f)) {
                    sunColor = Color(255, 150, 80);      // Orange sun at sunset/sunrise
                    sunGlowColor = Color(255, 100, 50);  // Red-orange glow
                } else {
                    sunColor = Color(200, 200, 255);     // Blue-white moon at night
                    sunGlowColor = Color(150, 150, 255); // Blue glow
                }
            } else {
                // Day - normal blue sky
                zenithColor = Color(100, 150, 255);  // Blue sky at top
                horizonColor = Color(220, 230, 255); // Lighter blue at horizon
                sunColor = Color(255, 253, 230);     // Yellowish sun
                sunGlowColor = Color(255, 200, 150); // Orange-ish glow
            }
        }
    }
};

// Main renderer class
class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();
    
    // Add move constructor and move assignment operator
    Renderer(Renderer&& other) noexcept;
    Renderer& operator=(Renderer&& other) noexcept;
    
    // Delete copy constructor and copy assignment operator (unique_ptr cannot be copied)
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    
    // Set up the renderer
    void initialize();
    
    // Render a frame
    void renderFrame(const BSPTree& bsp, const ViewPosition& view, const std::vector<Sprite>& sprites = {});
    
    // Get the rendered frame buffer
    const uint8_t* getFrameBuffer() const { return reinterpret_cast<const uint8_t*>(m_frameBuffer.data()); }
    
    // Get screen dimensions
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
    // Add a sprite to the scene (for dynamic sprite creation)
    void addSprite(const Sprite& sprite);
    
    // Clear all sprites
    void clearSprites();
    
    // Get the skybox
    Skybox& getSkybox() { return m_skybox; }
    
    // Enable/disable GPU acceleration
    void setGpuAccelerationEnabled(bool enabled) { m_gpuAccelerationEnabled = enabled; }
    bool isGpuAccelerationEnabled() const { return m_gpuAccelerationEnabled && m_cudaRenderer != nullptr; }
    
    // Minimap functionality
    void setMinimapEnabled(bool enabled) { m_minimapEnabled = enabled; }
    bool isMinimapEnabled() const { return m_minimapEnabled; }
    void setMinimapSize(int size) { m_minimapSize = size; }
    int getMinimapSize() const { return m_minimapSize; }
    void setMinimapPosition(int x, int y) { m_minimapX = x; m_minimapY = y; }
    void setMinimapScale(float scale) { m_minimapScale = scale; }
    
private:
    int m_width;
    int m_height;
    std::vector<Color> m_frameBuffer;
    std::vector<float> m_zBuffer;            // Depth buffer for each pixel
    std::vector<Texture> m_textures;         // Loaded textures
    std::vector<Sprite> m_sprites;           // Sprites to render
    Skybox m_skybox;                         // Skybox for background
    
    // Minimap properties
    bool m_minimapEnabled;                  // Whether to show the minimap
    int m_minimapSize;                      // Size of the minimap in pixels
    int m_minimapX;                         // X position of the minimap
    int m_minimapY;                         // Y position of the minimap
    float m_minimapScale;                   // Scale factor for minimap (world units to pixels)
    
    // CUDA acceleration
    bool m_gpuAccelerationEnabled;          // Flag for GPU acceleration
    std::unique_ptr<RendererCuda> m_cudaRenderer; // CUDA renderer
    
    // Wall Y coordinates for each column
    struct WallExtent {
        int top;        // Top pixel of wall
        int bottom;     // Bottom pixel of wall
        
        WallExtent() : top(0), bottom(0) {}
    };
    std::vector<WallExtent> m_wallExtents;   // Wall extents for each column
    std::vector<Visplane> m_visplanes;       // Visible floor/ceiling planes
    
    // DOOM-style rendering methods
    void clearBuffers();
    void renderBSP(const BSPTree& bsp, const ViewPosition& view);
    void renderWallSlice(const WallSlice& slice, const ViewPosition& view);
    
    // Skybox rendering
    void renderSkybox(const ViewPosition& view, float deltaTime);
    void drawSun(float screenX, float screenY, float size, const Color& color, float intensity);
    
    // Span-based floor and ceiling rendering (DOOM style)
    void renderFloorAndCeilingSpans(const BSPTree& bsp, const ViewPosition& view);
    void makeVisplanes(const BSPTree& bsp, const ViewPosition& view);
    void renderVisplane(const Visplane& visplane, const ViewPosition& view);
    void renderSpan(const Span& span);
    
    // Sprite rendering (billboarded)
    void renderSprites(const BSPTree& bsp, const ViewPosition& view, const std::vector<Sprite>& sprites);
    void renderSprite(const Sprite& sprite, const BSPTree& bsp, const ViewPosition& view, float distance);
    
    // Simple floor and ceiling fill (for comparison)
    void renderFloorAndCeilingSimple(const BSPTree& bsp, const ViewPosition& view);
    
    // Render minimap
    void renderMinimap(const BSPTree& bsp, const ViewPosition& view);
    void drawMinimapWall(int x1, int y1, int x2, int y2, const Color& color);
    void drawMinimapPlayer(int x, int y, float angle, const Color& color);
    Vec2 worldToMinimap(const Vec2& worldPos) const;
    
    // Helper methods for rendering
    void drawVerticalLine(int x, int y1, int y2, const Color& color);
    void drawPixel(int x, int y, const Color& color);
    void drawPixelWithDepth(int x, int y, float depth, const Color& color);
    bool isPixelVisible(int x, int y, float depth) const;
    void drawHorizontalLine(int y, int x1, int x2, const Color& color);
    
    // Calculate projected wall height
    float calculateWallHeight(float distance, float wallHeight) const;
    
    // Calculate screen space Y coordinate from wall height
    int calculateScreenY(float projHeight, float offset) const;
    
    // Convert world space to screen space
    Vec2 worldToScreen(const Vec2& worldPos, const ViewPosition& view) const;
    
    // Convert screen space to world space
    Vec2 screenToWorld(int x, int y, float z, const ViewPosition& view) const;
    
    // Get Z-buffer value
    float getDepth(int x, int y) const;
    
    // Set Z-buffer value
    void setDepth(int x, int y, float depth);
    
    // Load textures
    void loadTextures();
    
    // Load sprite textures
    void loadSpriteTextures();
    
    // Sort sprites by distance from viewer
    std::vector<SpriteRenderData> sortSprites(const std::vector<Sprite>& sprites, const ViewPosition& view) const;
    
    // Check if sprite is visible
    bool isSpriteVisible(const Sprite& sprite, const ViewPosition& view, float& distance) const;
};

} // namespace PureDoom

#endif // RENDERER_H 