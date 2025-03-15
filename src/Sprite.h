#ifndef SPRITE_H
#define SPRITE_H

#include "BSPTree.h"
#include <string>
#include <vector>
#include <memory>

namespace PureDoom {

// Sprite types (similar to DOOM)
enum class SpriteType {
    PROP,       // Static prop (no animation)
    ITEM,       // Collectible item
    ENEMY,      // Enemy character
    PROJECTILE, // Weapon projectile
    EFFECT,     // Visual effect (explosion, etc.)
    WEAPON,     // Player's weapon
    DECORATION  // Background decoration
};

// Sprite frame data
struct SpriteFrame {
    int textureId;      // Texture ID for this frame
    float width;        // Width in world units
    float height;       // Height in world units
    Vec2 offset;        // Offset from sprite center
    
    SpriteFrame() : textureId(0), width(1.0f), height(1.0f) {}
    SpriteFrame(int texId, float w, float h) 
        : textureId(texId), width(w), height(h) {}
};

// Sprite object (instance in the world)
class Sprite {
public:
    Sprite();
    Sprite(const Vec2& pos, float heightOffset, SpriteType type);
    ~Sprite() = default;
    
    // Basic properties
    Vec2 position;            // Position in world (x,y)
    float heightOffset;       // Vertical offset from floor
    float scale;              // Scale factor (1.0 = normal)
    int lightLevel;           // Light level (0-255)
    bool visible;             // Is the sprite visible?
    bool flipped;             // Should the sprite be flipped horizontally?
    bool alignToFloor;        // Should align to floor height?
    SpriteType type;          // Type of sprite
    std::string tag;          // Optional tag for identification
    int sectorId;             // Which sector the sprite is in (-1 if unknown)
    
    // Animation
    std::vector<SpriteFrame> frames;    // Animation frames
    int currentFrame;                   // Current frame index
    float animationSpeed;               // Frames per second
    float animationTimer;               // Time since last frame change
    bool animating;                     // Is animation playing?
    bool loopAnimation;                 // Should animation loop?
    
    // Initialize the sprite
    void init(const Vec2& pos, float heightOffset, SpriteType type);
    
    // Add a frame to the sprite
    void addFrame(int textureId, float width, float height);
    
    // Update animation
    void update(float deltaTime);
    
    // Get current frame
    const SpriteFrame& getCurrentFrame() const;
    
    // Set animation state
    void startAnimation(bool loop = true);
    void stopAnimation();
    void setFrame(int frameIndex);
    
    // Utility function to calculate sprite height based on sector
    float getWorldHeight(const BSPTree& bsp) const;
};

// Used for sorting sprites by distance
struct SpriteRenderData {
    const Sprite* sprite;    // Pointer to sprite
    float distance;          // Distance from viewer
    
    SpriteRenderData(const Sprite* s, float d) : sprite(s), distance(d) {}
    
    // Comparison operator for sorting (far to near)
    bool operator<(const SpriteRenderData& other) const {
        return distance > other.distance; // Note: sorting from far to near
    }
};

} // namespace PureDoom

#endif // SPRITE_H 