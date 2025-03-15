#include "Sprite.h"
#include <algorithm>

namespace PureDoom {

Sprite::Sprite() {
    init(Vec2(0.0f, 0.0f), 0.0f, SpriteType::PROP);
}

Sprite::Sprite(const Vec2& pos, float heightOffset, SpriteType type) {
    init(pos, heightOffset, type);
}

void Sprite::init(const Vec2& pos, float heightOffset, SpriteType type) {
    // Basic properties
    this->position = pos;
    this->heightOffset = heightOffset;
    this->type = type;
    this->scale = 1.0f;
    this->lightLevel = 255;
    this->visible = true;
    this->flipped = false;
    this->alignToFloor = true;
    this->sectorId = -1;
    
    // Animation properties
    this->currentFrame = 0;
    this->animationSpeed = 10.0f; // 10 FPS by default
    this->animationTimer = 0.0f;
    this->animating = false;
    this->loopAnimation = true;
    
    // Clear any existing frames
    this->frames.clear();
}

void Sprite::addFrame(int textureId, float width, float height) {
    frames.push_back(SpriteFrame(textureId, width, height));
}

void Sprite::update(float deltaTime) {
    // If not animating or no frames, nothing to update
    if (!animating || frames.empty()) {
        return;
    }
    
    // Update animation timer
    animationTimer += deltaTime;
    
    // Check if it's time for next frame
    float frameTime = 1.0f / animationSpeed;
    if (animationTimer >= frameTime) {
        // Advance to next frame
        currentFrame++;
        animationTimer -= frameTime;
        
        // Check if we've reached the end of animation
        if (currentFrame >= static_cast<int>(frames.size())) {
            if (loopAnimation) {
                // Loop back to beginning
                currentFrame = 0;
            } else {
                // Stop at last frame
                currentFrame = static_cast<int>(frames.size()) - 1;
                animating = false;
            }
        }
    }
}

const SpriteFrame& Sprite::getCurrentFrame() const {
    if (frames.empty()) {
        static const SpriteFrame defaultFrame;
        return defaultFrame;
    }
    
    return frames[std::min(currentFrame, static_cast<int>(frames.size()) - 1)];
}

void Sprite::startAnimation(bool loop) {
    if (frames.empty()) {
        return;
    }
    
    animating = true;
    loopAnimation = loop;
    
    // If we were at the end, restart from beginning
    if (currentFrame >= static_cast<int>(frames.size())) {
        currentFrame = 0;
    }
    
    animationTimer = 0.0f;
}

void Sprite::stopAnimation() {
    animating = false;
}

void Sprite::setFrame(int frameIndex) {
    if (frames.empty()) {
        return;
    }
    
    currentFrame = std::max(0, std::min(frameIndex, static_cast<int>(frames.size()) - 1));
    animationTimer = 0.0f;
}

float Sprite::getWorldHeight(const BSPTree& bsp) const {
    if (sectorId < 0) {
        // Try to find the sector if not known
        int foundSectorId = bsp.findSector(position);
        if (foundSectorId < 0) {
            // Fallback height if no sector found
            return heightOffset;
        }
        
        // Update the sector ID for next time
        const_cast<Sprite*>(this)->sectorId = foundSectorId;
    }
    
    // Get the sector
    const auto& sectors = bsp.getSectors();
    if (sectorId >= 0 && sectorId < static_cast<int>(sectors.size())) {
        const Sector& sector = sectors[sectorId];
        
        // If aligned to floor, use floor height plus offset
        if (alignToFloor) {
            return sector.floorHeight + heightOffset;
        } else {
            // Otherwise, use absolute height
            return heightOffset;
        }
    }
    
    // Fallback if sector is invalid
    return heightOffset;
}

} // namespace PureDoom 