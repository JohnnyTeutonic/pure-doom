#ifndef RENDERER_H
#define RENDERER_H

#include "BSPTree.h"
#include <vector>
#include <memory>
#include <array>
#include <cstdint>
#include <string>
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
class Texture;

// Color representation (RGBA)
struct Color {
    uint8_t r, g, b, a;
    
    Color() : r(0), g(0), b(0), a(255) {}
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}
    
    static Color fromHSV(float h, float s, float v);
    static Color blend(const Color& c1, const Color& c2, float t);
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
    
    WallSlice() : x(0), distance(0.0f), height(0.0f), texCoordU(0.0f),
                 textureId(-1), lightLevel(0), isPortal(false) {}
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
    
private:
    int m_width, m_height;
    std::vector<Color> m_pixels;
    
    void loadFromFile(const std::string& filename);
    void generateCheckerboard();
};

// Main renderer class
class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();
    
    // Set up the renderer
    void initialize();
    
    // Render a frame
    void renderFrame(const BSPTree& bsp, const ViewPosition& view);
    
    // Get the rendered frame buffer
    const uint8_t* getFrameBuffer() const { return reinterpret_cast<const uint8_t*>(m_frameBuffer.data()); }
    
    // Get screen dimensions
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
private:
    int m_width;
    int m_height;
    std::vector<Color> m_frameBuffer;
    std::vector<float> m_zBuffer;            // Depth buffer for each column
    std::vector<Texture> m_textures;         // Loaded textures
    
    // DOOM-style rendering methods
    void clearBuffers();
    void renderBSP(const BSPTree& bsp, const ViewPosition& view);
    void renderWallSlice(const WallSlice& slice);
    void renderFloorAndCeiling(const BSPTree& bsp, const ViewPosition& view);
    
    // Helper methods for rendering
    void drawVerticalLine(int x, int y1, int y2, const Color& color);
    void drawPixel(int x, int y, const Color& color);
    
    // Calculate projected wall height
    float calculateWallHeight(float distance, float wallHeight) const;
    
    // Calculate screen space Y coordinate from wall height
    int calculateScreenY(float projHeight, float offset) const;
    
    // Convert world space to screen space
    Vec2 worldToScreen(const Vec2& worldPos, const ViewPosition& view) const;
    
    // Load textures
    void loadTextures();
};

} // namespace PureDoom

#endif // RENDERER_H 