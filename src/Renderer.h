#ifndef RENDERER_H
#define RENDERER_H

#include "BSPTree.h"
#include "Sprite.h"
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
struct Span;
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
    float floorHeight;   // Floor height of sector
    float ceilingHeight; // Ceiling height of sector
    
    WallSlice() : x(0), distance(0.0f), height(0.0f), texCoordU(0.0f),
                 textureId(-1), lightLevel(0), isPortal(false),
                 floorHeight(0.0f), ceilingHeight(0.0f) {}
};

// Span for floor/ceiling rendering
struct Span {
    int y;             // Screen y coordinate
    int startX;        // Start x position
    int endX;          // End x position
    float startU;      // Start texture U coordinate
    float startV;      // Start texture V coordinate
    float endU;        // End texture U coordinate
    float endV;        // End texture V coordinate
    float startZ;      // Start depth
    float endZ;        // End depth
    float startUOverZ; // Start U/Z for perspective correction
    float startVOverZ; // Start V/Z for perspective correction
    float endUOverZ;   // End U/Z for perspective correction
    float endVOverZ;   // End V/Z for perspective correction
    float startInvZ;   // Start 1/Z for perspective correction
    float endInvZ;     // End 1/Z for perspective correction
    int textureId;     // Texture to use
    int lightLevel;    // Light level
    bool isFloor;      // Is this a floor span (vs ceiling)?
    
    Span() : y(0), startX(0), endX(0), 
             startU(0.0f), startV(0.0f), endU(0.0f), endV(0.0f),
             startZ(0.0f), endZ(0.0f), 
             startUOverZ(0.0f), startVOverZ(0.0f), endUOverZ(0.0f), endVOverZ(0.0f),
             startInvZ(0.0f), endInvZ(0.0f),
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

// Main renderer class
class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();
    
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
    
private:
    int m_width;
    int m_height;
    std::vector<Color> m_frameBuffer;
    std::vector<float> m_zBuffer;            // Depth buffer for each pixel
    std::vector<Texture> m_textures;         // Loaded textures
    std::vector<Sprite> m_sprites;           // Sprites to render
    
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