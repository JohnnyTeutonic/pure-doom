#include "RendererCuda.h"
#include "Renderer.h"
#include "BSPTree.h"
#include "Platform.h" // Include Platform.h
#include <iostream>
#include <vector>
#include <SDL.h>
#include <chrono>
#include <thread>

using namespace PureDoom;

// Simple texture creation for testing
Texture createSimpleTexture(int width, int height, uint8_t r, uint8_t g, uint8_t b) {
    Texture texture(width, height);
    
    // Fill with solid color
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            texture.m_pixels[y * width + x] = Color(r, g, b);
        }
    }
    
    return texture;
}

// Create checkerboard texture
Texture createCheckerboardTexture(int width, int height, Color color1, Color color2, int squareSize) {
    Texture texture(width, height);
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bool isColorOne = ((x / squareSize) + (y / squareSize)) % 2 == 0;
            texture.m_pixels[y * width + x] = isColorOne ? color1 : color2;
        }
    }
    
    return texture;
}

// Create a brick wall texture
Texture createBrickTexture(int width, int height, Color brickColor, Color mortarColor) {
    Texture texture(width, height);
    
    int brickWidth = width / 4;
    int brickHeight = height / 8;
    int mortarSize = 2;
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int brickX = x / brickWidth;
            int brickY = y / brickHeight;
            
            // Offset every other row
            if (brickY % 2 == 1) {
                brickX = (x + brickWidth / 2) / brickWidth;
            }
            
            // Check if we're on mortar or brick
            bool isHorizontalMortar = (y % brickHeight) < mortarSize || (y % brickHeight) >= (brickHeight - mortarSize);
            bool isVerticalMortar = (x % brickWidth) < mortarSize || (x % brickWidth) >= (brickWidth - mortarSize);
            
            if (isHorizontalMortar || isVerticalMortar) {
                texture.m_pixels[y * width + x] = mortarColor;
            } else {
                // Vary brick color slightly
                int variation = ((brickX * 10) + (brickY * 10)) % 30;
                Color variedColor(
                    std::min(255, brickColor.r + variation),
                    std::min(255, brickColor.g + variation / 2),
                    std::min(255, brickColor.b + variation / 2)
                );
                texture.m_pixels[y * width + x] = variedColor;
            }
        }
    }
    
    return texture;
}

// Create a DOOM-like floor texture
Texture createDoomFloorTexture(int width, int height) {
    Texture texture(width, height);
    
    // Base color (brownish gray)
    uint8_t baseR = 48;
    uint8_t baseG = 42;
    uint8_t baseB = 35;
    
    // Edge color (slightly lighter)
    uint8_t edgeR = 58;
    uint8_t edgeG = 52;
    uint8_t edgeB = 45;
    
    // Grid size (how many grid cells in the texture)
    int gridSize = 8;
    int cellWidth = width / gridSize;
    int cellHeight = height / gridSize;
    
    // Create a DOOM-like floor pattern
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Determine if we're on a grid edge
            bool isHorizontalEdge = (y % cellHeight) < 2 || (y % cellHeight) >= (cellHeight - 2);
            bool isVerticalEdge = (x % cellWidth) < 2 || (x % cellWidth) >= (cellWidth - 2);
            
            // Add some variation for a more organic look
            int noise = ((x * 13 + y * 7) % 20) - 10;
            
            if (isHorizontalEdge || isVerticalEdge) {
                // Edge color with noise
                texture.m_pixels[y * width + x] = Color(
                    static_cast<uint8_t>(std::max(0, std::min(255, edgeR + noise))),
                    static_cast<uint8_t>(std::max(0, std::min(255, edgeG + noise))),
                    static_cast<uint8_t>(std::max(0, std::min(255, edgeB + noise)))
                );
            } else {
                // Base color with noise
                texture.m_pixels[y * width + x] = Color(
                    static_cast<uint8_t>(std::max(0, std::min(255, baseR + noise))),
                    static_cast<uint8_t>(std::max(0, std::min(255, baseG + noise))),
                    static_cast<uint8_t>(std::max(0, std::min(255, baseB + noise)))
                );
            }
        }
    }
    
    return texture;
}

// Create a bloody wall texture (DOOM-like)
Texture createBloodyWallTexture(int width, int height, Color baseColor, Color bloodColor) {
    Texture texture(width, height);
    
    // Create a base stone/metal texture
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Add some noise to the base color
            int noise = ((x * 7 + y * 13) % 30) - 15;
            
            Color pixelColor(
                std::max(0, std::min(255, baseColor.r + noise)),
                std::max(0, std::min(255, baseColor.g + noise)),
                std::max(0, std::min(255, baseColor.b + noise))
            );
            
            texture.m_pixels[y * width + x] = pixelColor;
        }
    }
    
    // Add blood streaks and splatters
    int numStreaks = width / 8;
    for (int i = 0; i < numStreaks; ++i) {
        // Random starting position for the streak
        int startX = rand() % width;
        int startY = rand() % (height / 3);
        
        // Random length and width of the streak
        int streakLength = height / 2 + rand() % (height / 2);
        int streakWidth = 2 + rand() % 6;
        
        // Draw the blood streak downward
        for (int y = 0; y < streakLength; ++y) {
            int currentY = startY + y;
            if (currentY >= height) break;
            
            // Make the streak meander slightly
            int offsetX = (rand() % 5) - 2;
            
            // Vary the width as we go down
            int currentWidth = std::max(1, streakWidth - (y / (streakLength / 3)));
            
            for (int w = -currentWidth/2; w <= currentWidth/2; ++w) {
                int currentX = startX + offsetX + w;
                if (currentX >= 0 && currentX < width) {
                    // Fade the blood color as it goes down
                    float fade = 1.0f - (float)y / streakLength;
                    Color fadedBlood(
                        std::max(0, std::min(255, int(bloodColor.r * fade))),
                        std::max(0, std::min(255, int(bloodColor.g * fade))),
                        std::max(0, std::min(255, int(bloodColor.b * fade)))
                    );
                    
                    // Blend with existing color
                    Color& existingColor = texture.m_pixels[currentY * width + currentX];
                    existingColor = Color(
                        (existingColor.r + fadedBlood.r * 2) / 3,
                        (existingColor.g + fadedBlood.g * 2) / 3,
                        (existingColor.b + fadedBlood.b * 2) / 3
                    );
                }
            }
        }
    }
    
    // Add blood splatters
    int numSplatters = width / 10;
    for (int i = 0; i < numSplatters; ++i) {
        int centerX = rand() % width;
        int centerY = rand() % height;
        int radius = 3 + rand() % 8;
        
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                int currentX = centerX + x;
                int currentY = centerY + y;
                
                if (currentX >= 0 && currentX < width && currentY >= 0 && currentY < height) {
                    float distance = std::sqrt(x*x + y*y);
                    if (distance <= radius) {
                        float intensity = 1.0f - (distance / radius);
                        
                        // Add some randomness to the splatter edge
                        if (rand() % 100 < intensity * 100) {
                            Color& existingColor = texture.m_pixels[currentY * width + currentX];
                            existingColor = Color(
                                (existingColor.r + bloodColor.r * 3) / 4,
                                (existingColor.g + bloodColor.g * 3) / 4,
                                (existingColor.b + bloodColor.b * 3) / 4
                            );
                        }
                    }
                }
            }
        }
    }
    
    return texture;
}

// Create a hellish/fiery wall texture
Texture createHellishWallTexture(int width, int height) {
    Texture texture(width, height);
    
    // Create a base dark rock texture
    Color darkRock(40, 30, 25);
    Color lavaRock(80, 40, 30);
    
    // Create a cracked rock pattern
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Use Perlin-like noise simulation for rock texture
            float noiseVal = (std::sin(x * 0.1f) * std::cos(y * 0.1f) + 
                             std::sin(x * 0.05f + y * 0.05f) * std::cos(x * 0.06f - y * 0.03f)) * 0.5f + 0.5f;
            
            // Add some smaller detail noise
            float detailNoise = (std::sin(x * 0.4f) * std::cos(y * 0.4f)) * 0.2f + 0.8f;
            noiseVal *= detailNoise;
            
            // Determine if this is a crack
            bool isCrack = noiseVal < 0.4f;
            
            if (isCrack) {
                // Glowing crack - more intense at the center of the crack
                float glowIntensity = 1.0f - (noiseVal / 0.4f);
                
                // Create a fiery glow color
                Color glowColor(
                    std::min(255, int(200 * glowIntensity + 55)),
                    std::min(255, int(100 * glowIntensity + 30)),
                    std::min(255, int(50 * glowIntensity))
                );
                
                texture.m_pixels[y * width + x] = glowColor;
            } else {
                // Regular rock with some variation
                float rockBlend = (noiseVal - 0.4f) / 0.6f; // 0 to 1 range for non-crack areas
                
                Color rockColor(
                    int(darkRock.r * (1.0f - rockBlend) + lavaRock.r * rockBlend),
                    int(darkRock.g * (1.0f - rockBlend) + lavaRock.g * rockBlend),
                    int(darkRock.b * (1.0f - rockBlend) + lavaRock.b * rockBlend)
                );
                
                texture.m_pixels[y * width + x] = rockColor;
            }
        }
    }
    
    return texture;
}

// Create a flesh/organic wall texture (very DOOM-like)
Texture createFleshWallTexture(int width, int height) {
    Texture texture(width, height);
    
    // Base flesh colors
    Color fleshLight(180, 100, 90);
    Color fleshDark(120, 60, 50);
    Color veinColor(140, 20, 20);
    
    // Create the base flesh texture with veins
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Create a pulsating flesh pattern
            float baseNoise = (std::sin(x * 0.05f) * std::cos(y * 0.05f) + 
                              std::sin((x+y) * 0.08f) * std::cos((x-y) * 0.07f)) * 0.5f + 0.5f;
            
            // Add some smaller detail for skin texture
            float detailNoise = (std::sin(x * 0.2f + 10) * std::cos(y * 0.2f + 5)) * 0.3f + 0.7f;
            float combinedNoise = baseNoise * detailNoise;
            
            // Determine the flesh color based on the noise
            Color fleshColor(
                int(fleshDark.r * (1.0f - combinedNoise) + fleshLight.r * combinedNoise),
                int(fleshDark.g * (1.0f - combinedNoise) + fleshLight.g * combinedNoise),
                int(fleshDark.b * (1.0f - combinedNoise) + fleshLight.b * combinedNoise)
            );
            
            // Check if this should be a vein
            float veinNoise = std::sin(x * 0.1f + y * 0.15f) * std::cos(y * 0.1f - x * 0.05f);
            bool isVein = veinNoise > 0.7f && rand() % 10 > 5;
            
            if (isVein) {
                // Add some variation to the vein color
                int veinVariation = rand() % 30 - 15;
                Color currentVeinColor(
                    std::max(0, std::min(255, veinColor.r + veinVariation)),
                    std::max(0, std::min(255, veinColor.g + veinVariation / 2)),
                    std::max(0, std::min(255, veinColor.b + veinVariation / 2))
                );
                
                texture.m_pixels[y * width + x] = currentVeinColor;
            } else {
                texture.m_pixels[y * width + x] = fleshColor;
            }
        }
    }
    
    // Add some "wounds" or openings
    int numWounds = width / 16;
    for (int i = 0; i < numWounds; ++i) {
        int centerX = rand() % width;
        int centerY = rand() % height;
        int radiusX = 5 + rand() % 10;
        int radiusY = 5 + rand() % 10;
        
        for (int y = -radiusY; y <= radiusY; ++y) {
            for (int x = -radiusX; x <= radiusX; ++x) {
                int currentX = centerX + x;
                int currentY = centerY + y;
                
                if (currentX >= 0 && currentX < width && currentY >= 0 && currentY < height) {
                    // Create an oval shape
                    float normalizedX = (float)x / radiusX;
                    float normalizedY = (float)y / radiusY;
                    float distance = std::sqrt(normalizedX*normalizedX + normalizedY*normalizedY);
                    
                    if (distance <= 1.0f) {
                        // Darker in the center, redder at the edges
                        float edgeFactor = distance * 0.7f + 0.3f;
                        
                        Color woundColor(
                            int(veinColor.r * edgeFactor),
                            int(veinColor.g * distance * distance),
                            int(veinColor.b * distance * distance)
                        );
                        
                        texture.m_pixels[currentY * width + currentX] = woundColor;
                    }
                }
            }
        }
    }
    
    return texture;
}

// Create a molten rock wall texture for the elevated room
Texture createMoltenRockTexture(int width, int height) {
    Texture texture(width, height);
    
    // Base dark rock color
    Color darkRock(30, 20, 15);
    Color moltenRock(180, 60, 20);
    
    // Create the base rock texture with molten cracks
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Create a noise pattern for the rock
            float noiseVal = (std::sin(x * 0.15f) * std::cos(y * 0.15f) + 
                             std::sin((x+y) * 0.1f) * std::cos((x-y) * 0.08f)) * 0.5f + 0.5f;
            
            // Add some smaller detail for rock texture
            float detailNoise = (std::sin(x * 0.3f + 5) * std::cos(y * 0.3f + 7)) * 0.3f + 0.7f;
            float combinedNoise = noiseVal * detailNoise;
            
            // Determine if this is a molten crack
            bool isMolten = combinedNoise < 0.3f;
            
            if (isMolten) {
                // Molten lava effect - brighter in the center of the crack
                float glowIntensity = 1.0f - (combinedNoise / 0.3f);
                glowIntensity = glowIntensity * glowIntensity; // Square for more contrast
                
                // Create a molten glow color
                Color glowColor(
                    std::min(255, int(moltenRock.r * glowIntensity + 75)),
                    std::min(255, int(moltenRock.g * glowIntensity + 20)),
                    std::min(255, int(moltenRock.b * glowIntensity))
                );
                
                texture.m_pixels[y * width + x] = glowColor;
            } else {
                // Regular rock with some variation
                float rockBlend = (combinedNoise - 0.3f) / 0.7f; // 0 to 1 range for non-molten areas
                rockBlend = std::min(1.0f, std::max(0.0f, rockBlend)); // Clamp to 0-1
                
                // Add some variation to the rock color
                int variation = ((x * 7 + y * 13) % 20) - 10;
                
                Color rockColor(
                    std::max(0, std::min(255, int(darkRock.r + variation))),
                    std::max(0, std::min(255, int(darkRock.g + variation))),
                    std::max(0, std::min(255, int(darkRock.b + variation)))
                );
                
                texture.m_pixels[y * width + x] = rockColor;
            }
        }
    }
    
    // Add some glowing embers/sparks
    int numEmbers = width / 4;
    for (int i = 0; i < numEmbers; i++) {
        int centerX = rand() % width;
        int centerY = rand() % height;
        int size = 1 + rand() % 2;
        
        for (int y = -size; y <= size; y++) {
            for (int x = -size; x <= size; x++) {
                int currentX = centerX + x;
                int currentY = centerY + y;
                
                if (currentX >= 0 && currentX < width && currentY >= 0 && currentY < height) {
                    float distance = std::sqrt(x*x + y*y);
                    if (distance <= size) {
                        float intensity = 1.0f - (distance / size);
                        
                        // Only add embers with some randomness
                        if (rand() % 100 < 70) {
                            Color& pixel = texture.m_pixels[currentY * width + currentX];
                            pixel.r = std::min(255, int(pixel.r * 0.5f + 255 * intensity * 0.5f));
                            pixel.g = std::min(255, int(pixel.g * 0.5f + 180 * intensity * 0.5f));
                            pixel.b = std::min(255, int(pixel.b * 0.5f + 50 * intensity * 0.5f));
                        }
                    }
                }
            }
        }
    }
    
    return texture;
}

// Create a charred bone floor texture for the elevated room
Texture createCharredBoneFloorTexture(int width, int height) {
    Texture texture(width, height);
    
    // Base bone color (off-white)
    Color boneColor(220, 210, 190);
    // Charred color (dark gray with slight red tint)
    Color charredColor(40, 30, 25);
    
    // Create a grid pattern for bone tiles
    int gridSize = 8;
    int cellWidth = width / gridSize;
    int cellHeight = height / gridSize;
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Determine grid cell
            int cellX = x / cellWidth;
            int cellY = y / cellHeight;
            
            // Create a pattern of bone tiles
            float noiseVal = (std::sin(cellX * 1.5f) * std::cos(cellY * 1.5f) + 
                             std::sin((cellX+cellY) * 2.0f) * std::cos((cellX-cellY) * 1.7f)) * 0.5f + 0.5f;
            
            // Add some smaller detail noise
            float detailNoise = (std::sin(x * 0.2f) * std::cos(y * 0.2f)) * 0.3f + 0.7f;
            float combinedNoise = noiseVal * detailNoise;
            
            // Determine if this is a bone or charred area
            bool isCharred = combinedNoise < 0.4f;
            
            // Determine if we're on a grid edge (cracks between bones)
            bool isHorizontalCrack = (y % cellHeight) < 2 || (y % cellHeight) >= (cellHeight - 2);
            bool isVerticalCrack = (x % cellWidth) < 2 || (x % cellWidth) >= (cellWidth - 2);
            bool isCrack = isHorizontalCrack || isVerticalCrack;
            
            // Add some variation for a more organic look
            int noise = ((x * 13 + y * 7) % 20) - 10;
            
            if (isCrack) {
                // Cracks are always charred/dark
                texture.m_pixels[y * width + x] = Color(
                    std::max(0, std::min(255, charredColor.r + noise / 2)),
                    std::max(0, std::min(255, charredColor.g + noise / 2)),
                    std::max(0, std::min(255, charredColor.b + noise / 2))
                );
            } else if (isCharred) {
                // Charred bone areas
                texture.m_pixels[y * width + x] = Color(
                    std::max(0, std::min(255, charredColor.r + noise)),
                    std::max(0, std::min(255, charredColor.g + noise)),
                    std::max(0, std::min(255, charredColor.b + noise))
                );
            } else {
                // Regular bone areas
                texture.m_pixels[y * width + x] = Color(
                    std::max(0, std::min(255, boneColor.r + noise)),
                    std::max(0, std::min(255, boneColor.g + noise)),
                    std::max(0, std::min(255, boneColor.b + noise))
                );
            }
            
            // Add some small red spots (blood stains)
            if (rand() % 100 < 5) {
                int stainSize = 1 + rand() % 2;
                
                for (int dy = -stainSize; dy <= stainSize; dy++) {
                    for (int dx = -stainSize; dx <= stainSize; dx++) {
                        int px = x + dx;
                        int py = y + dy;
                        if (px >= 0 && px < width && py >= 0 && py < height) {
                            float dist = std::sqrt(dx*dx + dy*dy);
                            if (dist <= stainSize && rand() % 100 < 70) {
                                int idx = py * width + px;
                                texture.m_pixels[idx].r = std::min(255, texture.m_pixels[idx].r + 40);
                                texture.m_pixels[idx].g = std::max(0, texture.m_pixels[idx].g - 20);
                                texture.m_pixels[idx].b = std::max(0, texture.m_pixels[idx].b - 20);
                            }
                        }
                    }
                }
            }
        }
    }
    
    return texture;
}

// Create a pulsating flesh ceiling texture for the elevated room
Texture createPulsatingFleshCeilingTexture(int width, int height) {
    Texture texture(width, height);
    
    // Base flesh colors
    Color fleshDark(120, 40, 40);
    Color fleshLight(180, 80, 70);
    Color veinColor(90, 10, 10);
    
    // Create the base flesh texture with veins and pulsating areas
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Create a pulsating pattern
            float baseNoise = (std::sin(x * 0.07f) * std::cos(y * 0.07f) + 
                              std::sin((x+y) * 0.05f) * std::cos((x-y) * 0.06f)) * 0.5f + 0.5f;
            
            // Add some smaller detail for flesh texture
            float detailNoise = (std::sin(x * 0.3f + 10) * std::cos(y * 0.3f + 5)) * 0.3f + 0.7f;
            float combinedNoise = baseNoise * detailNoise;
            
            // Create pulsating effect (would animate if we could change it over time)
            float pulseEffect = std::sin(combinedNoise * 5.0f) * 0.5f + 0.5f;
            
            // Determine the flesh color based on the noise
            Color fleshColor(
                int(fleshDark.r * (1.0f - pulseEffect) + fleshLight.r * pulseEffect),
                int(fleshDark.g * (1.0f - pulseEffect) + fleshLight.g * pulseEffect),
                int(fleshDark.b * (1.0f - pulseEffect) + fleshLight.b * pulseEffect)
            );
            
            // Check if this should be a vein
            float veinNoise = std::sin(x * 0.15f + y * 0.2f) * std::cos(y * 0.15f - x * 0.1f);
            bool isVein = veinNoise > 0.7f && rand() % 10 > 6;
            
            if (isVein) {
                // Add some variation to the vein color
                int veinVariation = rand() % 20 - 10;
                Color currentVeinColor(
                    std::max(0, std::min(255, veinColor.r + veinVariation)),
                    std::max(0, std::min(255, veinColor.g + veinVariation / 2)),
                    std::max(0, std::min(255, veinColor.b + veinVariation / 2))
                );
                
                texture.m_pixels[y * width + x] = currentVeinColor;
            } else {
                texture.m_pixels[y * width + x] = fleshColor;
            }
        }
    }
    
    // Add some "pustules" or bulging areas
    int numPustules = width / 8;
    for (int i = 0; i < numPustules; ++i) {
        int centerX = rand() % width;
        int centerY = rand() % height;
        int radiusX = 3 + rand() % 6;
        int radiusY = 3 + rand() % 6;
        
        for (int y = -radiusY; y <= radiusY; ++y) {
            for (int x = -radiusX; x <= radiusX; ++x) {
                int currentX = centerX + x;
                int currentY = centerY + y;
                
                if (currentX >= 0 && currentX < width && currentY >= 0 && currentY < height) {
                    // Create an oval shape
                    float normalizedX = (float)x / radiusX;
                    float normalizedY = (float)y / radiusY;
                    float distance = std::sqrt(normalizedX*normalizedX + normalizedY*normalizedY);
                    
                    if (distance <= 1.0f) {
                        // Lighter in the center, darker at the edges
                        float edgeFactor = 1.0f - distance;
                        edgeFactor = edgeFactor * edgeFactor; // Square for more contrast
                        
                        Color pustuleColor(
                            std::min(255, int(fleshLight.r + 40 * edgeFactor)),
                            std::min(255, int(fleshLight.g + 20 * edgeFactor)),
                            std::min(255, int(fleshLight.b + 10 * edgeFactor))
                        );
                        
                        // Blend with existing color based on distance
                        Color& existingColor = texture.m_pixels[currentY * width + currentX];
                        float blendFactor = 0.7f * edgeFactor;
                        existingColor = Color(
                            int(existingColor.r * (1.0f - blendFactor) + pustuleColor.r * blendFactor),
                            int(existingColor.g * (1.0f - blendFactor) + pustuleColor.g * blendFactor),
                            int(existingColor.b * (1.0f - blendFactor) + pustuleColor.b * blendFactor)
                        );
                    }
                }
            }
        }
    }
    
    return texture;
}

// Create a ramp/staircase texture for the transition between side room and elevated room
Texture createRampTexture(int width, int height) {
    Texture texture(width, height);
    
    // Base colors
    Color baseColor(100, 80, 70);    // Brownish base
    Color stepColor(140, 60, 50);    // Reddish step color
    Color bloodColor(180, 20, 20);   // Blood accent color
    
    // Create a stair-like pattern with very distinct steps
    int numSteps = 5;  // Number of steps in the staircase
    int stepHeight = height / numSteps;
    
    for (int y = 0; y < height; ++y) {
        // Determine which step we're on (0 is bottom, numSteps-1 is top)
        int step = y / stepHeight;
        
        // Make the steps very distinct with clear horizontal lines
        bool isStepEdge = (y % stepHeight) < 3;
        bool isStepTop = (y % stepHeight) >= 3 && (y % stepHeight) < (stepHeight - 3);
        
        for (int x = 0; x < width; ++x) {
            // Add some variation to the color based on position
            int variation = ((x * 7 + y * 13) % 20) - 10;
            
            if (isStepEdge) {
                // Step edge is darker and has more blood - very distinct
                Color edgeColor(
                    std::max(0, std::min(255, stepColor.r + variation - 40)),
                    std::max(0, std::min(255, stepColor.g + variation - 40)),
                    std::max(0, std::min(255, stepColor.b + variation - 40))
                );
                texture.m_pixels[y * width + x] = edgeColor;
                
                // Add blood streaks on step edges
                if (rand() % 100 < 50) { // Increased blood for visibility
                    texture.m_pixels[y * width + x].r = std::min(255, texture.m_pixels[y * width + x].r + 60);
                    texture.m_pixels[y * width + x].g = std::max(0, texture.m_pixels[y * width + x].g - 30);
                    texture.m_pixels[y * width + x].b = std::max(0, texture.m_pixels[y * width + x].b - 30);
                }
            } else if (isStepTop) {
                // Regular step surface - lighter to contrast with edges
                Color currentColor(
                    std::max(0, std::min(255, baseColor.r + variation + step * 10 + 20)),
                    std::max(0, std::min(255, baseColor.g + variation + 10)),
                    std::max(0, std::min(255, baseColor.b + variation + 10))
                );
                texture.m_pixels[y * width + x] = currentColor;
            } else {
                // Back of step - medium tone
                Color backColor(
                    std::max(0, std::min(255, baseColor.r + variation + step * 5)),
                    std::max(0, std::min(255, baseColor.g + variation - 10)),
                    std::max(0, std::min(255, baseColor.b + variation - 10))
                );
                texture.m_pixels[y * width + x] = backColor;
            }
            
            // Add some random blood stains
            if (rand() % 100 < 8) {
                int stainSize = 1 + rand() % 3;
                
                for (int dy = -stainSize; dy <= stainSize; dy++) {
                    for (int dx = -stainSize; dx <= stainSize; dx++) {
                        int px = x + dx;
                        int py = y + dy;
                        if (px >= 0 && px < width && py >= 0 && py < height) {
                            float dist = std::sqrt(dx*dx + dy*dy);
                            if (dist <= stainSize && rand() % 100 < 70) {
                                int idx = py * width + px;
                                texture.m_pixels[idx].r = std::min(255, texture.m_pixels[idx].r + 60);
                                texture.m_pixels[idx].g = std::max(0, texture.m_pixels[idx].g - 30);
                                texture.m_pixels[idx].b = std::max(0, texture.m_pixels[idx].b - 30);
                            }
                        }
                    }
                }
            }
            
            // Add horizontal lines to emphasize steps
            if (y % stepHeight == 0 || y % stepHeight == stepHeight - 1) {
                texture.m_pixels[y * width + x] = Color(20, 10, 10); // Very dark line
            }
        }
    }
    
    return texture;
}

// Add this function to draw a path to the elevated room
void drawPathToElevatedRoom(SDL_Renderer* renderer, const Vec2& playerPos, int minimapX, int minimapY, int minimapSize, float scale) {
    // Define waypoints to the elevated room with updated coordinates
    const Vec2 waypoints[] = {
        Vec2(0.0f, 0.0f),      // Main room center
        Vec2(0.0f, 2.5f),      // Main room to corridor entrance
        Vec2(0.0f, 6.0f),      // Corridor to side room entrance
        Vec2(0.5f, 6.5f),      // Side room entrance
        Vec2(2.0f, 6.5f),      // Side room to staircase entrance
        Vec2(2.5f, 6.0f),      // Staircase to elevated room entrance
        Vec2(3.5f, 5.5f)       // Elevated room center
    };
    
    // Calculate minimap center (player will be centered)
    int centerX = minimapX + minimapSize / 2;
    int centerY = minimapY + minimapSize / 2;
    
    // Draw path lines
    SDL_SetRenderDrawColor(renderer, 255, 215, 0, 255); // Gold color for path
    
    // Draw lines connecting waypoints
    for (int i = 0; i < 6; i++) {
        // Convert world coordinates to minimap coordinates
        int x1 = centerX + (waypoints[i].x - playerPos.x) * scale;
        int y1 = centerY - (waypoints[i].y - playerPos.y) * scale;
        int x2 = centerX + (waypoints[i+1].x - playerPos.x) * scale;
        int y2 = centerY - (waypoints[i+1].y - playerPos.y) * scale;
        
        // Draw the path line
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        
        // Draw a small circle at each waypoint
        const int dotSize = 3;
        SDL_Rect dotRect = {x1 - dotSize/2, y1 - dotSize/2, dotSize, dotSize};
        SDL_RenderFillRect(renderer, &dotRect);
    }
    
    // Draw final waypoint
    int lastX = centerX + (waypoints[6].x - playerPos.x) * scale;
    int lastY = centerY - (waypoints[6].y - playerPos.y) * scale;
    const int dotSize = 3;
    SDL_Rect dotRect = {lastX - dotSize/2, lastY - dotSize/2, dotSize, dotSize};
    SDL_RenderFillRect(renderer, &dotRect);
    
    // Draw a star at the elevated room center
    const int starSize = 5;
    SDL_Rect starRect = {lastX - starSize/2, lastY - starSize/2, starSize, starSize};
    SDL_RenderFillRect(renderer, &starRect);
}

// Render a minimap to help navigate
void renderMinimap(SDL_Renderer* renderer, const std::vector<Sector>& sectors, const BSPTree& bsp, const Vec2& playerPos, float playerAngle) {
    // Define minimap position and size
    int minimapSize = 150;
    int minimapX = 10;
    int minimapY = 10;
    
    // Define minimap scale (units to pixels)
    const float MINIMAP_SCALE = 15.0f; // Reduced scale to fit larger rooms
    
    // Define sector colors
    const SDL_Color sectorColors[] = {
        {100, 100, 255, 255},  // Main Room - Blue
        {100, 255, 100, 255},  // Corridor - Green
        {255, 100, 100, 255},  // Side Room - Red
        {255, 255, 100, 255},  // Elevated Room - Yellow
        {255, 100, 255, 255}   // Staircase - Purple
    };
    
    // Calculate minimap center (player will be centered)
    int centerX = minimapX + minimapSize / 2;
    int centerY = minimapY + minimapSize / 2;
    
    // Draw minimap background
    SDL_Rect minimapRect = {minimapX, minimapY, minimapSize, minimapSize};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(renderer, &minimapRect);
    
    // Draw minimap border
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &minimapRect);
    
    // Draw path to elevated room
    drawPathToElevatedRoom(renderer, playerPos, minimapX, minimapY, minimapSize, MINIMAP_SCALE);
    
    // Draw walls for each sector
    for (const Sector& sector : sectors) {
        // Choose color based on sector type
        if (sector.tag == "main_room") {
            SDL_SetRenderDrawColor(renderer, 100, 100, 255, 255); // Blue for main room
        } else if (sector.tag == "corridor") {
            SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255); // Green for corridor
        } else if (sector.tag == "side_room") {
            SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255); // Red for side room
        } else if (sector.tag == "staircase") {
            SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255); // Yellow for staircase
        } else if (sector.tag == "elevated_room") {
            SDL_SetRenderDrawColor(renderer, 255, 100, 255, 255); // Purple for elevated room
        } else {
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255); // Gray for other sectors
        }
        
        // Draw walls
        for (const Wall& wall : sector.walls) {
            // Convert world coordinates to minimap coordinates
            int x1 = centerX + (wall.segment.start.position.x - playerPos.x) * MINIMAP_SCALE;
            int y1 = centerY - (wall.segment.start.position.y - playerPos.y) * MINIMAP_SCALE;
            int x2 = centerX + (wall.segment.end.position.x - playerPos.x) * MINIMAP_SCALE;
            int y2 = centerY - (wall.segment.end.position.y - playerPos.y) * MINIMAP_SCALE;
            
            // Draw the wall line
            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
            
            // If this is a portal, draw it differently
            if (wall.isTransparent) {
                // Draw a thicker line for portals
                for (int i = -1; i <= 1; i++) {
                    SDL_RenderDrawLine(renderer, x1 + i, y1 + i, x2 + i, y2 + i);
                }
            }
        }
    }
    
    // Draw player position
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect playerRect = {centerX - 3, centerY - 3, 6, 6};
    SDL_RenderFillRect(renderer, &playerRect);
    
    // Draw player direction line
    int dirX = centerX + cos(playerAngle) * 15;
    int dirY = centerY - sin(playerAngle) * 15;
    SDL_RenderDrawLine(renderer, centerX, centerY, dirX, dirY);
    
    // Draw minimap legend
    int legendY = minimapY + minimapSize + 10;
    SDL_Rect legendRect = {minimapX, legendY, minimapSize, 60};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(renderer, &legendRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &legendRect);
    
    // Draw legend entries
    int entryY = legendY + 5;
    for (int i = 0; i < 5; i++) {
        SDL_SetRenderDrawColor(renderer, sectorColors[i].r, sectorColors[i].g, sectorColors[i].b, sectorColors[i].a);
        SDL_Rect colorRect = {minimapX + 5, entryY, 10, 10};
        SDL_RenderFillRect(renderer, &colorRect);
        
        std::string sectorName;
        switch (i) {
            case 0: sectorName = "Main Room"; break;
            case 1: sectorName = "Corridor"; break;
            case 2: sectorName = "Side Room"; break;
            case 3: sectorName = "Elevated Room"; break;
            case 4: sectorName = "Staircase"; break;
        }
        
        // Draw sector name (we'll use a rectangle to represent text since we can't easily render text)
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect textRect = {minimapX + 20, entryY, static_cast<int>(sectorName.length() * 5), 10};
        SDL_RenderDrawRect(renderer, &textRect);
        
        entryY += 12;
    }
    
    // Draw platforms on the minimap
    const std::vector<Platform>& platforms = bsp.getPlatforms();
    for (const Platform& platform : platforms) {
        // Set color based on platform type
        if (platform.type == PlatformType::STAIR) {
            // Bright yellow for stairs
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        } else {
            // Cyan for other platforms
            SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
        }
        
        // Draw platform outline
        for (size_t i = 0; i < platform.vertices.size(); i++) {
            size_t j = (i + 1) % platform.vertices.size();
            
            // Convert world coordinates to minimap coordinates (player-relative)
            int x1 = centerX + (platform.vertices[i].x - playerPos.x) * MINIMAP_SCALE;
            int y1 = centerY - (platform.vertices[i].y - playerPos.y) * MINIMAP_SCALE;
            int x2 = centerX + (platform.vertices[j].x - playerPos.x) * MINIMAP_SCALE;
            int y2 = centerY - (platform.vertices[j].y - playerPos.y) * MINIMAP_SCALE;
            
            // Draw the line
            SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        }
        
        // Fill platform with semi-transparent color
        std::vector<SDL_Point> points;
        for (const Vec2& vertex : platform.vertices) {
            SDL_Point point;
            point.x = centerX + (vertex.x - playerPos.x) * MINIMAP_SCALE;
            point.y = centerY - (vertex.y - playerPos.y) * MINIMAP_SCALE;
            points.push_back(point);
        }
        
        // Draw filled polygon (simplified - just draw lines between points)
        for (size_t i = 0; i < points.size(); i++) {
            size_t j = (i + 1) % points.size();
            SDL_RenderDrawLine(renderer, points[i].x, points[i].y, points[j].x, points[j].y);
        }
    }
    
    // Draw player position and direction
    // ... existing code ...
}

// Create an elevated platform for the test map
Platform createElevatedPlatform() {
    // Create a rectangular platform in the main room
    std::vector<Vec2> platformVertices;
    
    // Define the platform shape (rectangular, counter-clockwise order)
    platformVertices.push_back(Vec2(-1.5f, -1.5f));  // Bottom-left
    platformVertices.push_back(Vec2(1.5f, -1.5f));   // Bottom-right
    platformVertices.push_back(Vec2(1.5f, 0.0f));    // Top-right
    platformVertices.push_back(Vec2(-1.5f, 0.0f));   // Top-left
    
    // Create the platform with appropriate parameters
    Platform platform(
        platformVertices,    // Vertices defining the platform shape
        0.5f,               // Height above the floor (0.5 units)
        0.2f,               // Thickness of the platform (0.2 units)
        0,                  // Top texture ID (floor texture)
        1,                  // Bottom texture ID (ceiling texture)
        3,                  // Side texture ID (wall texture)
        180,                // Light level
        0                   // Sector ID (main room)
    );
    
    // Set a tag for the platform
    platform.tag = "main_room_platform";
    
    return platform;
}

// Create a texture that gives the illusion of stairs
Texture createStairIllusionTexture(int width, int height, int numSteps) {
    Texture texture(width, height);
    
    // Calculate step height
    int stepHeight = height / numSteps;
    
    // Colors for steps
    Color stepColor(100, 90, 80);       // Base step color
    Color stepEdgeColor(140, 130, 120); // Lighter color for step edges
    Color shadowColor(60, 50, 40);      // Darker color for shadows
    
    // Draw the steps
    for (int step = 0; step < numSteps; step++) {
        int yStart = step * stepHeight;
        int yEnd = (step + 1) * stepHeight;
        
        // Draw the horizontal part of the step (top surface)
        for (int y = yStart; y < yStart + stepHeight * 0.7; y++) {
            for (int x = 0; x < width; x++) {
                // Add some noise to the texture
                int noise = rand() % 20 - 10;
                
                // Add a gradient from back to front
                float gradient = 1.0f - (float)(y - yStart) / (stepHeight * 0.7f);
                
                // Calculate pixel color with noise and gradient
                Color pixelColor = stepColor;
                pixelColor.r = std::min(255, std::max(0, pixelColor.r + noise + int(gradient * 30)));
                pixelColor.g = std::min(255, std::max(0, pixelColor.g + noise + int(gradient * 30)));
                pixelColor.b = std::min(255, std::max(0, pixelColor.b + noise + int(gradient * 20)));
                
                // Add horizontal lines for step detail
                if ((x + step * 5) % 10 < 2) {
                    pixelColor.r = std::max(0, pixelColor.r - 10);
                    pixelColor.g = std::max(0, pixelColor.g - 10);
                    pixelColor.b = std::max(0, pixelColor.b - 10);
                }
                
                texture.m_pixels[y * width + x] = pixelColor;
            }
        }
        
        // Draw the vertical part of the step (riser)
        for (int y = yStart + stepHeight * 0.7; y < yEnd; y++) {
            for (int x = 0; x < width; x++) {
                // Add some noise to the texture
                int noise = rand() % 15 - 7;
                
                // Shadow effect for the riser
                Color pixelColor = shadowColor;
                pixelColor.r = std::min(255, std::max(0, pixelColor.r + noise));
                pixelColor.g = std::min(255, std::max(0, pixelColor.g + noise));
                pixelColor.b = std::min(255, std::max(0, pixelColor.b + noise));
                
                texture.m_pixels[y * width + x] = pixelColor;
            }
        }
        
        // Draw a highlight line at the edge of each step
        int edgeY = yStart + stepHeight * 0.7 - 1;
        for (int x = 0; x < width; x++) {
            texture.m_pixels[edgeY * width + x] = stepEdgeColor;
        }
    }
    
    return texture;
}

// Create a texture that gives the illusion of looking up/down stairs from first-person view
Texture createStairPerspectiveTexture(int width, int height, bool lookingUp, int numSteps) {
    Texture texture(width, height);
    
    // Background color (dark)
    Color backgroundColor(30, 25, 20);
    
    // Fill with background color first
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            texture.m_pixels[y * width + x] = backgroundColor;
        }
    }
    
    // Colors for steps
    Color stepColor(100, 90, 80);       // Base step color
    Color stepEdgeColor(140, 130, 120); // Lighter color for step edges
    Color shadowColor(60, 50, 40);      // Darker color for shadows
    
    // Calculate perspective parameters
    float vanishingPointY = lookingUp ? height * 0.3f : height * 0.7f;
    float stepWidthFactor = width * 0.8f;
    
    // Draw steps with perspective
    for (int step = 0; step < numSteps; step++) {
        // Calculate step position with perspective
        float distanceFactor = (float)(step + 1) / numSteps;
        float perspectiveFactor = lookingUp ? distanceFactor : 1.0f - distanceFactor;
        
        // Calculate step dimensions with perspective
        int stepWidth = stepWidthFactor * (1.0f - perspectiveFactor * 0.7f);
        int stepHeight = height * 0.1f * (1.0f - perspectiveFactor * 0.7f);
        
        // Calculate step position
        int stepX = (width - stepWidth) / 2;
        int stepY;
        
        if (lookingUp) {
            // When looking up, steps appear higher with distance
            stepY = (int)(vanishingPointY - step * stepHeight * 1.5f);
        } else {
            // When looking down, steps appear lower with distance
            stepY = (int)(vanishingPointY + step * stepHeight * 1.5f);
        }
        
        // Draw the step (horizontal part)
        for (int y = stepY; y < stepY + stepHeight; y++) {
            if (y < 0 || y >= height) continue;
            
            for (int x = stepX; x < stepX + stepWidth; x++) {
                if (x < 0 || x >= width) continue;
                
                // Add some noise and lighting
                int noise = rand() % 15 - 7;
                float lightFactor = lookingUp ? 
                    1.0f - (float)(y - stepY) / stepHeight : 
                    (float)(y - stepY) / stepHeight;
                
                Color pixelColor = stepColor;
                pixelColor.r = std::min(255, std::max(0, pixelColor.r + noise + int(lightFactor * 30)));
                pixelColor.g = std::min(255, std::max(0, pixelColor.g + noise + int(lightFactor * 30)));
                pixelColor.b = std::min(255, std::max(0, pixelColor.b + noise + int(lightFactor * 20)));
                
                texture.m_pixels[y * width + x] = pixelColor;
            }
        }
        
        // Draw step edge (highlight)
        int edgeY = lookingUp ? stepY + stepHeight - 1 : stepY;
        for (int x = stepX; x < stepX + stepWidth; x++) {
            if (edgeY >= 0 && edgeY < height && x >= 0 && x < width) {
                texture.m_pixels[edgeY * width + x] = stepEdgeColor;
            }
        }
        
        // Draw vertical part (riser)
        int riserStartY, riserEndY;
        if (lookingUp) {
            riserStartY = stepY + stepHeight;
            riserEndY = (step < numSteps - 1) ? 
                (int)(vanishingPointY - (step + 1) * stepHeight * 1.5f) : 
                riserStartY + stepHeight;
        } else {
            riserEndY = stepY;
            riserStartY = (step < numSteps - 1) ? 
                (int)(vanishingPointY + (step + 1) * stepHeight * 1.5f) : 
                riserEndY - stepHeight;
        }
        
        for (int y = riserStartY; y < riserEndY; y++) {
            if (y < 0 || y >= height) continue;
            
            for (int x = stepX; x < stepX + stepWidth; x++) {
                if (x < 0 || x >= width) continue;
                
                // Add some noise
                int noise = rand() % 10 - 5;
                
                Color pixelColor = shadowColor;
                pixelColor.r = std::min(255, std::max(0, pixelColor.r + noise));
                pixelColor.g = std::min(255, std::max(0, pixelColor.g + noise));
                pixelColor.b = std::min(255, std::max(0, pixelColor.b + noise));
                
                texture.m_pixels[y * width + x] = pixelColor;
            }
        }
    }
    
    return texture;
}

// Create a set of DOOM-like stairs using platforms
std::vector<Platform> createDoomStairs(const Vec2& start, const Vec2& end, float baseHeight, 
                                      float stepHeight, int numSteps, 
                                      int topTex, int bottomTex, int sideTex, int light, int sector) {
    std::vector<Platform> stairs;
    
    // Create each stair step as a separate platform
    for (int i = 0; i < numSteps; ++i) {
        Platform step = Platform::createStair(start, end, baseHeight, stepHeight, i, numSteps, 
                                             topTex, bottomTex, sideTex, light, sector);
        stairs.push_back(step);
    }
    
    return stairs;
}

// Create a large platform that covers the entire room
Platform createRoomPlatform(float height, int topTex, int bottomTex, int sideTex, int light, int sector) {
    // Create a rectangular platform covering the main room
    std::vector<Vec2> platformVertices;
    
    // Define the platform shape (rectangular, counter-clockwise order)
    // Using larger dimensions to cover the entire room
    platformVertices.push_back(Vec2(-5.0f, -5.0f));  // Bottom-left
    platformVertices.push_back(Vec2(5.0f, -5.0f));   // Bottom-right
    platformVertices.push_back(Vec2(5.0f, 5.0f));    // Top-right
    platformVertices.push_back(Vec2(-5.0f, 5.0f));   // Top-left
    
    // Create the platform with appropriate parameters
    Platform platform(
        platformVertices,    // Vertices defining the platform shape
        height,             // Height above the floor
        0.2f,               // Thickness of the platform
        topTex,             // Top texture ID
        bottomTex,          // Bottom texture ID
        sideTex,            // Side texture ID
        light,              // Light level
        sector              // Sector ID
    );
    
    // Set a tag for the platform
    platform.tag = "room_platform";
    
    return platform;
}

// Main function to test our CUDA test map
int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Create window
    const int WIDTH = 1024;
    const int HEIGHT = 768;
    SDL_Window* window = SDL_CreateWindow(
        "DOOM-Like Hellish Test Map",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Create renderer
    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    
    if (!sdlRenderer) {
        std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Create texture for rendering the frame
    SDL_Texture* frameTexture = SDL_CreateTexture(
        sdlRenderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        WIDTH, HEIGHT
    );
    
    if (!frameTexture) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(sdlRenderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Create CUDA renderer
    RendererCuda cudaRenderer(WIDTH, HEIGHT);
    if (!cudaRenderer.isCudaAvailable()) {
        std::cerr << "CUDA is not available. Exiting." << std::endl;
        SDL_DestroyTexture(frameTexture);
        SDL_DestroyRenderer(sdlRenderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    if (!cudaRenderer.initialize()) {
        std::cerr << "Failed to initialize CUDA renderer. Exiting." << std::endl;
        SDL_DestroyTexture(frameTexture);
        SDL_DestroyRenderer(sdlRenderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // Initialize mouse for relative movement
    SDL_SetRelativeMouseMode(SDL_TRUE);
    std::cout << "Mouse control enabled" << std::endl;
    
    // Create texture vector
    std::vector<Texture> textures;
    
    // Set skybox properties
    Skybox skybox;
    skybox.zenithColor = Color(80, 20, 10); // Dark red at the top
    skybox.horizonColor = Color(200, 60, 20); // Fiery orange at the horizon
    skybox.maxViewDistance = 30.0f;
    skybox.dynamicSky = true;
    skybox.sunAngle = 1.0f;  // Position in radians
    skybox.sunHeight = 0.2f; // Lower in the sky (0.0 = horizon, 1.0 = zenith)
    skybox.sunSize = 10.0f;  // Larger sun for a more imposing presence
    skybox.sunColor = Color(255, 180, 50); // Bright yellow-orange sun
    skybox.sunGlowColor = Color(255, 80, 10); // Intense fiery red glow
    skybox.sunGlowSize = 8.0f; // Larger glow for more dramatic effect
    
    // Set up light ray parameters
    const int numLightRays = 24; // Increased number of light rays for better coverage
    const float rayLength = 20.0f; // Increased length of rays to reach the map
    const float rayWidth = 0.5f; // Slightly narrower rays for more defined shadows
    const float rayIntensity = 0.85f; // Intensity of rays
    
    // Parameters for map lighting
    const float mapLightingIntensity = 1.2f; // Stronger light effect on the map
    const float lightAttenuation = 0.15f; // How quickly light fades with distance (lower = less attenuation)
    const float shadowDarkness = 0.8f; // How dark the shadows are (0-1)
    const float shadowSoftness = 0.3f; // How soft the shadow edges are (0-1)
    
    // Custom sun rendering with light rays
    cudaRenderer.setSkybox(skybox);
    cudaRenderer.setCustomParameter("enableHellSun", 1.0f); // Enable the custom hell sun
    cudaRenderer.setCustomParameter("numLightRays", static_cast<float>(numLightRays));
    cudaRenderer.setCustomParameter("rayLength", rayLength);
    cudaRenderer.setCustomParameter("rayWidth", rayWidth);
    cudaRenderer.setCustomParameter("rayIntensity", rayIntensity);
    cudaRenderer.setCustomParameter("enableShadowCasting", 1.0f); // Enable shadow casting
    cudaRenderer.setCustomParameter("shadowIntensity", shadowDarkness); // Shadow darkness (0-1)
    cudaRenderer.setCustomParameter("shadowSoftness", shadowSoftness); // Shadow softness (0-1)
    cudaRenderer.setCustomParameter("mapLightingIntensity", mapLightingIntensity); // Intensity of light on map
    cudaRenderer.setCustomParameter("lightAttenuation", lightAttenuation); // Distance attenuation for light
    
    // Create frame buffer and z-buffer
    std::vector<Color> frameBuffer(WIDTH * HEIGHT, Color(0, 0, 0));
    std::vector<float> zBuffer(WIDTH * HEIGHT, 1.0f);
    
    // Create textures for our test map
    
    // 0: Floor texture (DOOM-like floor pattern)
    textures.push_back(createDoomFloorTexture(64, 64));
    
    // 1: Ceiling texture (dark with embers)
    Texture ceilingTexture = createSimpleTexture(64, 64, 30, 25, 35);
    // Add some ember/fire particles to the ceiling
    for (int i = 0; i < 50; i++) {
        int x = rand() % 64;
        int y = rand() % 64;
        int size = 1 + rand() % 2;
        int brightness = 100 + rand() % 155;
        
        for (int dy = -size; dy <= size; dy++) {
            for (int dx = -size; dx <= size; dx++) {
                int px = x + dx;
                int py = y + dy;
                if (px >= 0 && px < 64 && py >= 0 && py < 64) {
                    float dist = std::sqrt(dx*dx + dy*dy);
                    if (dist <= size) {
                        float intensity = 1.0f - (dist / size);
                        ceilingTexture.m_pixels[py * 64 + px] = Color(
                            brightness * intensity,
                            brightness * intensity * 0.6f,
                            brightness * intensity * 0.3f
                        );
                    }
                }
            }
        }
    }
    textures.push_back(ceilingTexture);
    
    // 2: Side room floor texture (DOOM-like reddish floor)
    Texture sideRoomFloor = createDoomFloorTexture(64, 64);
    // Tint it redder and add blood stains
    for (int i = 0; i < sideRoomFloor.width() * sideRoomFloor.height(); i++) {
        Color& pixel = sideRoomFloor.m_pixels[i];
        pixel.r = std::min(255, pixel.r + 40);
        pixel.g = std::max(0, pixel.g - 10);
        pixel.b = std::max(0, pixel.b - 10);
        
        // Random blood stains
        if (rand() % 100 < 5) {
            int stainSize = 1 + rand() % 3;
            int x = i % sideRoomFloor.width();
            int y = i / sideRoomFloor.width();
            
            for (int dy = -stainSize; dy <= stainSize; dy++) {
                for (int dx = -stainSize; dx <= stainSize; dx++) {
                    int px = x + dx;
                    int py = y + dy;
                    if (px >= 0 && px < sideRoomFloor.width() && py >= 0 && py < sideRoomFloor.height()) {
                        float dist = std::sqrt(dx*dx + dy*dy);
                        if (dist <= stainSize && rand() % 100 < 70) {
                            int idx = py * sideRoomFloor.width() + px;
                            sideRoomFloor.m_pixels[idx].r = std::min(255, sideRoomFloor.m_pixels[idx].r + 30);
                            sideRoomFloor.m_pixels[idx].g = std::max(0, sideRoomFloor.m_pixels[idx].g - 20);
                            sideRoomFloor.m_pixels[idx].b = std::max(0, sideRoomFloor.m_pixels[idx].b - 20);
                        }
                    }
                }
            }
        }
    }
    textures.push_back(sideRoomFloor);
    
    // 3: Main room wall texture (bloody wall)
    textures.push_back(createBloodyWallTexture(128, 128, Color(80, 70, 60), Color(180, 20, 20)));
    
    // 4: Portal texture (hellish energy)
    Texture portalTexture(64, 64);
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            // Create swirling energy effect
            float angle = std::atan2(y - 32, x - 32);
            float distance = std::sqrt((x - 32) * (x - 32) + (y - 32) * (y - 32));
            float time_factor = 0.0f; // This would animate if we could change it over time
            
            float swirl = (angle + distance * 0.1f + time_factor) * 3.0f;
            float red_intensity = (std::sin(swirl) + 1.0f) * 0.5f;
            float green_intensity = (std::sin(swirl + 2.0f) + 1.0f) * 0.2f;
            float blue_intensity = (std::sin(swirl + 4.0f) + 1.0f) * 0.3f;
            
            // Fade intensity based on distance from center
            float edge_fade = 1.0f - std::min(1.0f, distance / 32.0f);
            edge_fade = edge_fade * edge_fade; // Square it for sharper falloff
            
            portalTexture.m_pixels[y * 64 + x] = Color(
                std::min(255, int(200 * red_intensity * edge_fade + 50)),
                std::min(255, int(100 * green_intensity * edge_fade + 10)),
                std::min(255, int(150 * blue_intensity * edge_fade + 30))
            );
        }
    }
    textures.push_back(portalTexture);
    
    // 5: Corridor wall texture (hellish wall)
    textures.push_back(createHellishWallTexture(64, 64));
    
    // 6: Side room wall texture (flesh wall)
    textures.push_back(createFleshWallTexture(64, 64));
    
    // 7: Elevated room wall texture (molten rock)
    textures.push_back(createMoltenRockTexture(64, 64));
    
    // 8: Elevated room floor texture (charred bone)
    textures.push_back(createCharredBoneFloorTexture(64, 64));
    
    // 9: Elevated room ceiling texture (pulsating flesh)
    textures.push_back(createPulsatingFleshCeilingTexture(64, 64));
    
    // 10: Staircase texture (ramp-like)
    textures.push_back(createRampTexture(64, 64));
    
    // 11: Stair illusion texture (side view)
    textures.push_back(createStairIllusionTexture(128, 128, 8));
    
    // 12: Stair perspective texture (looking up)
    textures.push_back(createStairPerspectiveTexture(128, 128, true, 8));
    
    // 13: Stair perspective texture (looking down)
    textures.push_back(createStairPerspectiveTexture(128, 128, false, 8));
    
    // Debug output for texture upload status
    std::cout << "--------- DEBUG TEXTURE INFORMATION ---------" << std::endl;
    std::cout << "Texture upload successful: " << (cudaRenderer.areTexturesUploaded() ? "YES" : "NO") << std::endl;
    std::cout << "Number of textures created: " << textures.size() << std::endl;
    std::cout << "Number of textures uploaded: " << cudaRenderer.getNumTextures() << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
    
    // Initialize view position near the center of main room
    ViewPosition view;
    view.position.x = 0.0f;
    view.position.y = 0.0f;
    view.height = 0.8f;
    view.angle = 0.0f;  // Facing north
    view.fov = 90.0f;   // Field of view in degrees
    
    // Create sectors for the test map (matching the CUDA test map structure)
    std::vector<Sector> testMapSectors;
    
    // Create a proper staircase in front of the player (DOOM-style)
    // In DOOM, stairs are created using sectors with different floor heights connected by portals
    
    // Main room sector
    Sector mainRoom;
    mainRoom.floorHeight = 0.0f;
    mainRoom.ceilingHeight = 2.0f;
    mainRoom.floorTextureId = 0;
    mainRoom.ceilingTextureId = 1;
    mainRoom.lightLevel = 200;
    mainRoom.tag = "main_room";
    
    // Main room walls (10x10 square, centered at origin)
    mainRoom.walls.push_back(Wall(Line(Vertex(-5.0f, 5.0f), Vertex(-1.0f, 5.0f)), 0, -1, 3));
    
    // Portal to corridor - make it wider
    Wall portalWall = Wall(Line(Vertex(-1.0f, 5.0f), Vertex(1.0f, 5.0f)), 0, 1, 4);
    portalWall.isTransparent = true;
    portalWall.isSolid = false;
    mainRoom.walls.push_back(portalWall);
    
    // Rest of main room walls
    mainRoom.walls.push_back(Wall(Line(Vertex(1.0f, 5.0f), Vertex(5.0f, 5.0f)), 0, -1, 3));
    mainRoom.walls.push_back(Wall(Line(Vertex(5.0f, 5.0f), Vertex(5.0f, -5.0f)), 0, -1, 3));
    mainRoom.walls.push_back(Wall(Line(Vertex(5.0f, -5.0f), Vertex(-5.0f, -5.0f)), 0, -1, 3));
    
    // South wall with stair illusion textures
    // Left section
    mainRoom.walls.push_back(Wall(Line(Vertex(-5.0f, -5.0f), Vertex(-2.0f, -5.0f)), 0, -1, 3));
    
    // Middle section with stair illusion texture (looking up)
    Wall stairWallUp = Wall(Line(Vertex(-2.0f, -5.0f), Vertex(-1.0f, -5.0f)), 0, -1, 12);
    stairWallUp.textureOffsetY = 0.0f; // Adjust texture alignment
    mainRoom.walls.push_back(stairWallUp);
    
    // Middle section with side view stair texture
    Wall stairWallSide = Wall(Line(Vertex(-1.0f, -5.0f), Vertex(1.0f, -5.0f)), 0, -1, 11);
    stairWallSide.textureOffsetY = 0.0f; // Adjust texture alignment
    mainRoom.walls.push_back(stairWallSide);
    
    // Middle section with stair illusion texture (looking down)
    Wall stairWallDown = Wall(Line(Vertex(1.0f, -5.0f), Vertex(2.0f, -5.0f)), 0, -1, 13);
    stairWallDown.textureOffsetY = 0.0f; // Adjust texture alignment
    mainRoom.walls.push_back(stairWallDown);
    
    // Right section
    mainRoom.walls.push_back(Wall(Line(Vertex(2.0f, -5.0f), Vertex(5.0f, -5.0f)), 0, -1, 3));
    
    // Replace a portion of the south wall with a portal to the new hellish sector
    // Remove the last wall we just added (south wall segment)
    mainRoom.walls.pop_back();
    
    // Add shortened south wall segments with a portal in between
    mainRoom.walls.push_back(Wall(Line(Vertex(2.0f, -5.0f), Vertex(3.0f, -5.0f)), 0, -1, 3));
    
    // Portal to hellish sector
    Wall hellPortal = Wall(Line(Vertex(3.0f, -5.0f), Vertex(4.0f, -5.0f)), 0, 5, 4); // Connect to new sector (5)
    hellPortal.isTransparent = true;
    hellPortal.isSolid = false;
    mainRoom.walls.push_back(hellPortal);
    
    // Continue the rest of the south wall
    mainRoom.walls.push_back(Wall(Line(Vertex(4.0f, -5.0f), Vertex(5.0f, -5.0f)), 0, -1, 3));
    
    // Add the main room to sectors
    testMapSectors.push_back(mainRoom);
    
    // Create a large platform in the center of the main room
    Platform centerPlatform;
    
    // Define the platform shape (rectangular, counter-clockwise order)
    std::vector<Vec2> platformVertices;
    platformVertices.push_back(Vec2(-3.0f, -3.0f));  // Bottom-left
    platformVertices.push_back(Vec2(3.0f, -3.0f));   // Bottom-right
    platformVertices.push_back(Vec2(3.0f, 3.0f));    // Top-right
    platformVertices.push_back(Vec2(-3.0f, 3.0f));   // Top-left
    
    // Create the platform with a much greater height to ensure visibility
    centerPlatform = Platform(platformVertices, 1.5f, 0.2f, 3, 3, 3, 255, 0);
    centerPlatform.type = PlatformType::STATIC;
    centerPlatform.isVisible = true;
    centerPlatform.isSolid = true;
    centerPlatform.tag = "center_platform";
    
    // Corridor sector - make it much larger
    Sector corridor;
    corridor.floorHeight = 0.0f;
    corridor.ceilingHeight = 2.0f; // Higher ceiling
    corridor.floorTextureId = 0;
    corridor.ceilingTextureId = 1;
    corridor.lightLevel = 150;
    corridor.tag = "corridor";
    
    // Corridor walls
    // Connection to main room - wider entrance
    Wall corridorEntrance = Wall(Line(Vertex(1.0f, 2.5f), Vertex(-1.0f, 2.5f)), 1, 0, 4);
    corridorEntrance.isTransparent = true;
    corridorEntrance.isSolid = false;
    corridor.walls.push_back(corridorEntrance);
    
    // Rest of corridor walls - much wider corridor
    corridor.walls.push_back(Wall(Line(Vertex(-1.0f, 2.5f), Vertex(-1.5f, 6.0f)), 1, -1, 5));
    corridor.walls.push_back(Wall(Line(Vertex(-1.5f, 6.0f), Vertex(-1.0f, 6.0f)), 1, -1, 5));
    
    // Portal to side room - wider entrance
    Wall sideRoomPortal = Wall(Line(Vertex(-1.0f, 6.0f), Vertex(1.0f, 6.0f)), 1, 2, 4);
    sideRoomPortal.isTransparent = true;
    sideRoomPortal.isSolid = false;
    corridor.walls.push_back(sideRoomPortal);
    
    // Last corridor wall
    corridor.walls.push_back(Wall(Line(Vertex(1.0f, 6.0f), Vertex(1.5f, 6.0f)), 1, -1, 5));
    corridor.walls.push_back(Wall(Line(Vertex(1.5f, 6.0f), Vertex(1.0f, 2.5f)), 1, -1, 5));
    
    // Side room sector - make it much larger
    Sector sideRoom;
    sideRoom.floorHeight = 0.1f;
    sideRoom.ceilingHeight = 2.2f; // Higher ceiling
    sideRoom.floorTextureId = 2;
    sideRoom.ceilingTextureId = 1;
    sideRoom.lightLevel = 120; // Brighter
    sideRoom.tag = "side_room";
    
    // Side room walls
    // Connection to corridor - wider entrance
    Wall sideRoomEntrance = Wall(Line(Vertex(1.0f, 6.0f), Vertex(-1.0f, 6.0f)), 2, 1, 4);
    sideRoomEntrance.isTransparent = true;
    sideRoomEntrance.isSolid = false;
    sideRoom.walls.push_back(sideRoomEntrance);
    
    // Rest of side room walls - much larger room
    sideRoom.walls.push_back(Wall(Line(Vertex(-1.0f, 6.0f), Vertex(-2.5f, 8.0f)), 2, -1, 6));
    sideRoom.walls.push_back(Wall(Line(Vertex(-2.5f, 8.0f), Vertex(2.5f, 8.0f)), 2, -1, 6));
    sideRoom.walls.push_back(Wall(Line(Vertex(2.5f, 8.0f), Vertex(2.0f, 6.5f)), 2, -1, 6));
    
    // Modified wall - side room now connects to staircase instead of directly to elevated room
    // Make the staircase entrance wider and more obvious
    Wall staircaseEntrance = Wall(Line(Vertex(2.0f, 6.5f), Vertex(1.0f, 6.0f)), 2, 4, 4); // Connect to staircase (sector 4)
    staircaseEntrance.isTransparent = true;
    staircaseEntrance.isSolid = false;
    sideRoom.walls.push_back(staircaseEntrance);
    
    // Create a staircase sector (new sector between side room and elevated room)
    Sector staircase;
    staircase.floorHeight = 0.2f;  // Slightly higher than side room
    staircase.ceilingHeight = 2.5f; // Higher ceiling to accommodate stairs
    staircase.floorTextureId = 10;  // Ramp/staircase texture
    staircase.ceilingTextureId = 1;
    staircase.lightLevel = 110;     // Brighter to make it more visible
    staircase.tag = "staircase";
    
    // Staircase walls - make it wider for easier navigation
    // Connection to side room
    Wall staircaseSideRoomEntrance = Wall(Line(Vertex(1.0f, 6.0f), Vertex(1.5f, 6.0f)), 4, 2, 4);
    staircaseSideRoomEntrance.isTransparent = true;
    staircaseSideRoomEntrance.isSolid = false;
    staircase.walls.push_back(staircaseSideRoomEntrance);
    
    // Staircase side walls - make them more visually distinct with different textures
    staircase.walls.push_back(Wall(Line(Vertex(1.5f, 6.0f), Vertex(2.5f, 6.0f)), 4, -1, 5)); // Use hellish wall texture
    
    // Connection to elevated room - make it wider
    Wall staircaseElevatedRoomExit = Wall(Line(Vertex(2.5f, 6.0f), Vertex(2.5f, 4.5f)), 4, 3, 4);
    staircaseElevatedRoomExit.isTransparent = true;
    staircaseElevatedRoomExit.isSolid = false;
    staircase.walls.push_back(staircaseElevatedRoomExit);
    
    // Last staircase wall - use a different texture to make it stand out
    staircase.walls.push_back(Wall(Line(Vertex(2.5f, 4.5f), Vertex(1.5f, 4.5f)), 4, -1, 6)); // Use flesh wall texture
    
    // Elevated room sector (higher than other rooms)
    Sector elevatedRoom;
    elevatedRoom.floorHeight = 0.5f;  // Higher floor - more noticeable difference
    elevatedRoom.ceilingHeight = 3.0f; // Much higher ceiling to make it feel more spacious
    elevatedRoom.floorTextureId = 8;   // Charred bone floor
    elevatedRoom.ceilingTextureId = 9; // Pulsating flesh ceiling
    elevatedRoom.lightLevel = 120;     // Brighter to make it more distinct
    elevatedRoom.tag = "elevated_room";
    
    // Elevated room walls
    // Connection to staircase (modified to connect to staircase instead of side room)
    Wall elevatedRoomEntrance = Wall(Line(Vertex(2.5f, 4.5f), Vertex(2.5f, 6.0f)), 3, 4, 4);
    elevatedRoomEntrance.isTransparent = true;
    elevatedRoomEntrance.isSolid = false;
    // Make sure the portal is not solid
    elevatedRoomEntrance.isSolid = false;
    elevatedRoom.walls.push_back(elevatedRoomEntrance);
    
    // Rest of elevated room walls (extending further out)
    elevatedRoom.walls.push_back(Wall(Line(Vertex(2.5f, 6.0f), Vertex(4.0f, 7.0f)), 3, -1, 7));
    elevatedRoom.walls.push_back(Wall(Line(Vertex(4.0f, 7.0f), Vertex(4.0f, 4.5f)), 3, -1, 7));
    elevatedRoom.walls.push_back(Wall(Line(Vertex(4.0f, 4.5f), Vertex(2.5f, 5.0f)), 3, -1, 7));
    
    // Add sectors to the collection
    testMapSectors.push_back(mainRoom);
    testMapSectors.push_back(corridor);
    testMapSectors.push_back(sideRoom);
    testMapSectors.push_back(elevatedRoom);
    testMapSectors.push_back(staircase);  // Add the new staircase sector
    
    // Create a new hellish sector extending south from the main room
    Sector hellishSector;
    hellishSector.floorHeight = -0.5f;  // Lower floor for a pit-like feel
    hellishSector.ceilingHeight = 3.0f; // Higher ceiling for an imposing atmosphere
    hellishSector.floorTextureId = 0;  // Using existing floor texture for now
    hellishSector.ceilingTextureId = 1; // Using existing ceiling texture for now
    hellishSector.lightLevel = 80;      // Darker with dramatic lighting
    hellishSector.tag = "hellish_pit";
    
    // Entrance from main room
    Wall hellEntrance = Wall(Line(Vertex(4.0f, -5.0f), Vertex(3.0f, -5.0f)), 5, 0, 4);
    hellEntrance.isTransparent = true;
    hellEntrance.isSolid = false;
    hellishSector.walls.push_back(hellEntrance);
    
    // Define the walls of the hellish sector (extending south in a rough pentagram shape)
    hellishSector.walls.push_back(Wall(Line(Vertex(3.0f, -5.0f), Vertex(2.0f, -8.0f)), 5, -1, 7));  // Left edge (molten rock)
    hellishSector.walls.push_back(Wall(Line(Vertex(2.0f, -8.0f), Vertex(3.0f, -10.0f)), 5, -1, 6)); // Left bottom (flesh wall)
    hellishSector.walls.push_back(Wall(Line(Vertex(3.0f, -10.0f), Vertex(4.0f, -11.0f)), 5, -1, 5)); // Bottom left (fiery wall)
    hellishSector.walls.push_back(Wall(Line(Vertex(4.0f, -11.0f), Vertex(5.0f, -10.0f)), 5, -1, 5)); // Bottom right (fiery wall)
    hellishSector.walls.push_back(Wall(Line(Vertex(5.0f, -10.0f), Vertex(6.0f, -8.0f)), 5, -1, 6)); // Right bottom (flesh wall)
    hellishSector.walls.push_back(Wall(Line(Vertex(6.0f, -8.0f), Vertex(4.0f, -5.0f)), 5, -1, 7));  // Right edge (molten rock)
    
    // Add hellish sector to the test map
    testMapSectors.push_back(hellishSector);
    
    // Build the BSP tree for collision detection
    BSPTree collisionBSP;
    collisionBSP.build(testMapSectors);
    
    // Create and add the elevated platform to the BSP tree
    Platform elevatedPlatform = createElevatedPlatform();
    collisionBSP.addPlatform(elevatedPlatform);
    
    // Create and add DOOM-like stairs to the BSP tree
    Vec2 stairsStart(-1.5f, -2.3f);  // Start position (slightly in front of south wall)
    Vec2 stairsEnd(1.5f, -2.3f);     // End position (wider and slightly in front of wall)
    float stairsBaseHeight = 0.0f;   // Start at floor level
    float stairsStepHeight = 0.4f;   // Increased height of each step (was 0.3f)
    int stairsNumSteps = 5;          // Number of steps
    
    // Instead of stairs, create multiple platforms covering the entire room at different heights
    std::vector<Platform> roomPlatforms;
    
    // Create platforms at different heights
    for (int i = 0; i < 5; i++) {
        float platformHeight = 0.2f + (i * 0.4f); // Increasing heights
        Platform roomPlatform = createRoomPlatform(
            platformHeight,
            3,  // Top texture (valid texture ID)
            3,  // Bottom texture (valid texture ID)
            3,  // Side texture (valid texture ID)
            255 - (i * 20), // Light level (decreasing brightness for higher platforms)
            0   // Sector ID (main room)
        );
        roomPlatforms.push_back(roomPlatform);
    }
    
    // Add each platform to the BSP tree
    for (const Platform& platform : roomPlatforms) {
        collisionBSP.addPlatform(platform);
    }
    
    // Add the center platform to the BSP tree
    collisionBSP.addPlatform(centerPlatform);
    
    // Create a pentagram platform in the center of the hellish sector
    Platform pentagramPlatform;
    
    // Define the platform shape (pentagram, counter-clockwise order)
    std::vector<Vec2> pentagramVertices;
    pentagramVertices.push_back(Vec2(4.0f, -7.0f));   // Top
    pentagramVertices.push_back(Vec2(5.0f, -8.5f));   // Right upper
    pentagramVertices.push_back(Vec2(4.5f, -10.0f));  // Right lower
    pentagramVertices.push_back(Vec2(3.5f, -10.0f));  // Left lower
    pentagramVertices.push_back(Vec2(3.0f, -8.5f));   // Left upper
    
    // Create the pentagram platform with a red glow
    pentagramPlatform = Platform(pentagramVertices, -0.3f, 0.1f, 5, 5, 5, 200, 0);
    pentagramPlatform.type = PlatformType::STATIC;
    pentagramPlatform.isVisible = true;
    pentagramPlatform.isSolid = true;
    pentagramPlatform.tag = "pentagram_platform";
    
    // Add the pentagram platform to the BSP tree
    collisionBSP.addPlatform(pentagramPlatform);
    
    // Add blood pools as additional platforms in the hellish sector
    Platform bloodPool1;
    std::vector<Vec2> bloodPool1Vertices;
    bloodPool1Vertices.push_back(Vec2(2.5f, -7.0f));
    bloodPool1Vertices.push_back(Vec2(3.5f, -7.5f));
    bloodPool1Vertices.push_back(Vec2(3.0f, -8.5f));
    bloodPool1Vertices.push_back(Vec2(2.0f, -8.0f));
    
    bloodPool1 = Platform(bloodPool1Vertices, -0.48f, 0.05f, 6, 6, 6, 150, 0);
    bloodPool1.type = PlatformType::STATIC;
    bloodPool1.isVisible = true;
    bloodPool1.isSolid = false; // Can walk through blood
    bloodPool1.tag = "blood_pool_1";
    collisionBSP.addPlatform(bloodPool1);
    
    Platform bloodPool2;
    std::vector<Vec2> bloodPool2Vertices;
    bloodPool2Vertices.push_back(Vec2(5.0f, -7.0f));
    bloodPool2Vertices.push_back(Vec2(5.5f, -8.0f));
    bloodPool2Vertices.push_back(Vec2(4.5f, -8.5f));
    bloodPool2Vertices.push_back(Vec2(4.0f, -7.5f));
    
    bloodPool2 = Platform(bloodPool2Vertices, -0.48f, 0.05f, 6, 6, 6, 150, 0);
    bloodPool2.type = PlatformType::STATIC;
    bloodPool2.isVisible = true;
    bloodPool2.isSolid = false; // Can walk through blood
    bloodPool2.tag = "blood_pool_2";
    collisionBSP.addPlatform(bloodPool2);
    
    // Add a demonic altar in the center of the pentagram
    Platform altar;
    std::vector<Vec2> altarVertices;
    altarVertices.push_back(Vec2(3.8f, -8.3f));
    altarVertices.push_back(Vec2(4.2f, -8.3f));
    altarVertices.push_back(Vec2(4.2f, -8.7f));
    altarVertices.push_back(Vec2(3.8f, -8.7f));
    
    altar = Platform(altarVertices, -0.2f, 0.3f, 7, 7, 7, 255, 0);
    altar.type = PlatformType::STATIC;
    altar.isVisible = true;
    altar.isSolid = true;
    altar.tag = "demonic_altar";
    collisionBSP.addPlatform(altar);
    
    // Print a helpful message about the platforms
    std::cout << "\n=== ROOM-COVERING PLATFORMS GUIDE ===\n";
    std::cout << "Multiple room-covering platforms have been added to the main room.\n";
    std::cout << "These platforms are stacked at different heights from 0.2 to 1.8 units.\n";
    std::cout << "Unlike the stair illusion textures, these are actual elevated platforms that you can walk on.\n";
    
    std::cout << "\nA large center platform has been added to the main room.\n";
    std::cout << "This platform is 6x6 units in size and rises 0.5 units above the floor.\n";
    std::cout << "It's positioned in the center of the room with plenty of space around it.\n";
    
    std::cout << "\nTo explore the platforms:\n";
    std::cout << "1. From the starting position, look around to see the different platform levels\n";
    std::cout << "2. Move onto a platform to change your height\n";
    std::cout << "3. Try jumping between different platform heights\n";
    std::cout << "4. Notice how each platform covers the entire room at a different height\n";
    
    // Print a helpful message about the stair illusion
    std::cout << "\n=== STAIR ILLUSION GUIDE ===\n";
    std::cout << "In addition to the real 3D stairs, we've also created an illusion of stairs\n";
    std::cout << "using special textures on the south wall of the main room.\n";
    std::cout << "The wall has three sections with different stair perspectives:\n";
    std::cout << "1. Left section: Stairs going up (first-person perspective)\n";
    std::cout << "2. Middle section: Side view of stairs\n";
    std::cout << "3. Right section: Stairs going down (first-person perspective)\n";
    std::cout << "This demonstrates how to create the illusion of 3D elements in a 2.5D engine.\n";
    std::cout << "Compare these texture-based illusions with the real 3D stairs you can walk on!\n";
    
    // Create sprites for the test map
    std::vector<Sprite> testSprites;
    
    // Print a helpful message about the elevated room
    std::cout << "\n=== NAVIGATION GUIDE ===\n";
    std::cout << "To reach the elevated room:\n";
    std::cout << "1. Go through the wide corridor to the north\n";
    std::cout << "2. From the corridor, enter the large side room to the north\n";
    std::cout << "3. In the side room, look for the staircase entrance in the eastern part of the room\n";
    std::cout << "4. Climb the staircase to reach the elevated room\n";
    std::cout << "\nTo find the stair illusion:\n";
    std::cout << "1. From the starting position, turn around (180 degrees)\n";
    std::cout << "2. Look at the south wall of the main room\n";
    std::cout << "3. You'll see three sections with different stair perspectives\n";
    
    // Use all sectors for the CUDA renderer
    cudaRenderer.useTestMapWithSectors(testMapSectors);
    
    // Main loop variables
    bool running = true;
    bool keyW = false, keyA = false, keyS = false, keyD = false;
    bool keyQ = false, keyE = false;
    bool keySpace = false, keyC = false; // For jumping and crouching
    SDL_Event event;
    
    // Mouse control variables
    bool mouseControlEnabled = true;
    const float mouseSensitivity = 0.002f;
    
    // Movement speed
    const float moveSpeed = 0.1f;
    const float turnSpeed = 0.05f;
    
    // Player radius for collision detection
    const float PLAYER_RADIUS = 0.3f;
    
    // Jump physics variables
    const float PLAYER_DEFAULT_HEIGHT = 0.8f;
    const float PLAYER_CROUCH_HEIGHT = 0.4f;
    const float JUMP_INITIAL_VELOCITY = 0.08f;
    const float GRAVITY = 0.004f;
    float verticalVelocity = 0.0f;
    bool isJumping = false;
    bool isCrouching = false;
    
    // For calculating deltaTime
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    // Add this near the top of the main() function before the main loop
    std::cout << "\n==== DOOM-LIKE TEST MAP INFORMATION ====\n";
    std::cout << "Hellish test map enabled, player at: (0, 0)\n";
    std::cout << "Map contains:\n";
    std::cout << "  - Main room (5x5) with bloody walls and portal to corridor\n";
    std::cout << "  - Hellish corridor with fiery cracks leading to side room\n";
    std::cout << "  - Flesh-walled side room with elevated floor\n";
    std::cout << "  - Proper staircase with 5 steps leading up to the elevated room\n";
    std::cout << "  - Elevated room with molten rock walls and charred bone floor\n";
    std::cout << "  - New hellish pit extending south with lava floor and burning ceiling\n";
    std::cout << "  - Stair illusion textures on the south wall of the main room\n";
    std::cout << "Textures:\n";
    std::cout << "  - DOOM-style floor (ID 0)\n";
    std::cout << "  - Ember-lit ceiling (ID 1)\n";
    std::cout << "  - Bloody floor for side room (ID 2)\n";
    std::cout << "  - Bloody wall texture (ID 3)\n";
    std::cout << "  - Hellish energy portal (ID 4)\n";
    std::cout << "  - Fiery cracked wall (ID 5)\n";
    std::cout << "  - Flesh wall with wounds (ID 6)\n";
    std::cout << "  - Molten rock wall (ID 7)\n";
    std::cout << "  - Charred bone floor (ID 8)\n";
    std::cout << "  - Pulsating flesh ceiling (ID 9)\n";
    std::cout << "  - Bloody staircase/steps (ID 10)\n";
    std::cout << "Controls:\n";
    std::cout << "  - WASD: Move around\n";
    std::cout << "  - QE: Rotate view (can be used alongside mouse)\n";
    std::cout << "  - Mouse: Look around (mouse control enabled by default)\n";
    std::cout << "  - Left Mouse Button: Interact with objects in front of you\n";
    std::cout << "  - Right Mouse Button: Secondary action\n";
    std::cout << "  - Mouse Wheel: Weapon selection (future functionality)\n";
    std::cout << "  - M: Toggle mouse control\n";
    std::cout << "  - SPACE: Jump\n";
    std::cout << "  - C: Crouch\n";
    std::cout << "  - P: Debug wall info\n";
    std::cout << "  - L: Debug sector info\n";
    std::cout << "  - T: Teleport to stair illusion textures (for testing)\n";
    std::cout << "  - O: Teleport to center platform (for testing)\n";
    std::cout << "  - ESC: Quit\n";
    std::cout << "Collision detection enabled with player radius: " << PLAYER_RADIUS << "\n";
    std::cout << "=========================================\n";
    
    // Add these variables near the top of the main loop
    float screenShakeAmount = 0.0f;
    float screenShakeDecay = 0.9f;
    
    // Upload textures to CUDA renderer after all textures have been created
    cudaRenderer.uploadTextures(textures);
    
    // Main loop
    while (running) {
        // Calculate deltaTime for smooth movement and physics
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        // Set relative mouse mode at the start of each frame if mouse control is enabled
        SDL_SetRelativeMouseMode(mouseControlEnabled ? SDL_TRUE : SDL_FALSE);
        
        // Calculate movement vector based on keyboard input
        Vec2 movementVector(0.0f, 0.0f);
        
        // Movement speed affected by crouch state
        float currentMoveSpeed = moveSpeed;
        if (isCrouching) {
            currentMoveSpeed *= 0.5f; // Move slower when crouched
        }
        
        if (keyW) {
            movementVector.x += currentMoveSpeed * cos(view.angle);
            movementVector.y += currentMoveSpeed * sin(view.angle);
        }
        if (keyS) {
            movementVector.x -= currentMoveSpeed * cos(view.angle);
            movementVector.y -= currentMoveSpeed * sin(view.angle);
        }
        if (keyA) {
            movementVector.x += currentMoveSpeed * cos(view.angle - M_PI / 2);
            movementVector.y += currentMoveSpeed * sin(view.angle - M_PI / 2);
        }
        if (keyD) {
            movementVector.x += currentMoveSpeed * cos(view.angle + M_PI / 2);
            movementVector.y += currentMoveSpeed * sin(view.angle + M_PI / 2);
        }
        
        // Track which sector the player is in for portal transition detection
        int currentSector = collisionBSP.findSector(view.position);
        static int previousSector = -1;
        
        // Check if player moved to a different sector (through a portal)
        if (currentSector != previousSector && currentSector >= 0) {
            std::string sectorName;
            switch (currentSector) {
                case 0: sectorName = "Main Room"; break;
                case 1: sectorName = "Corridor"; break;
                case 2: sectorName = "Side Room"; break;
                case 3: sectorName = "Elevated Room"; break;
                case 4: sectorName = "Staircase"; break;
                case 5: sectorName = "Hellish Pit"; break;
                default: sectorName = "Unknown Sector"; break;
            }
            std::cout << "You are in sector: " << sectorName << " (ID: " << currentSector << ")\n";
            
            // Special messages for specific sectors
            if (currentSector == 3) {
                std::cout << "NOTICE: You are in the elevated room. Floor height: " << testMapSectors[currentSector].floorHeight << "\n";
            }
            else if (currentSector == 4) {
                std::cout << "NOTICE: You are on the staircase. Floor height: " << testMapSectors[currentSector].floorHeight << "\n";
            }
            else if (currentSector == 5) {
                std::cout << "NOTICE: You have entered the hellish pit. The air is thick with sulfur and the heat is unbearable.\n";
                std::cout << "You can hear distant screams and the bubbling of molten lava beneath the floor.\n";
                
                // Add a screen shake effect when entering the hellish sector
                screenShakeAmount = 0.5f;
            }
            
            // Display floor and ceiling heights
            std::cout << "Floor height: " << testMapSectors[currentSector].floorHeight << "\n";
            std::cout << "Ceiling height: " << testMapSectors[currentSector].ceilingHeight << "\n";
            
            // Display light level
            std::cout << "Light level: " << testMapSectors[currentSector].lightLevel << "\n";
            
            // Display sector tag if available
            if (!testMapSectors[currentSector].tag.empty()) {
                std::cout << "Sector tag: " << testMapSectors[currentSector].tag << "\n";
            }
            
            // If player is entering the elevated room, provide a hint about the elevation
            if (currentSector == 3 && previousSector != 3) {
                std::cout << "NOTICE: You have reached the top of the stairs (height 0.5)." << std::endl;
                std::cout << "SOUND EFFECT: *footsteps on bone floor*" << std::endl;
                
                // Add a small screen shake when entering the elevated room
                screenShakeAmount = 0.3f;
            }
            
            // If player is entering the staircase, provide a hint
            if (currentSector == 4 && (previousSector == 2 || previousSector == 3)) {
                std::cout << "NOTICE: You are on the staircase. Climb " 
                          << (previousSector == 2 ? "up" : "down") << " the steps." << std::endl;
                std::cout << "SOUND EFFECT: *footsteps on stairs*" << std::endl;
                
                // Add a small screen shake when entering the staircase
                screenShakeAmount = 0.2f;
            }
            
            previousSector = currentSector;
        }
        
        // If player is in the side room and near the portal to the staircase, simulate a ramp effect
        if (currentSector == 2) {
            // Check if player is near the portal to the staircase
            const Wall& portal = testMapSectors[2].walls[3]; // The portal wall in side room
            float distToPortal = portal.segment.distanceToPoint(view.position);
            
            if (distToPortal < 1.0f) {
                // Calculate how close the player is to the portal (0 = at portal, 1 = far from portal)
                float portalProximity = std::min(1.0f, distToPortal);
                
                // Adjust player height based on proximity to portal
                if (!isJumping) {
                    float sideRoomHeight = 0.1f;
                    float staircaseHeight = 0.2f;
                    float heightDifference = staircaseHeight - sideRoomHeight;
                    
                    // The closer to the portal, the higher the player should be
                    float heightAdjustment = heightDifference * (1.0f - portalProximity);
                    view.height = PLAYER_DEFAULT_HEIGHT + heightAdjustment;
                    
                    // Visual feedback for climbing the first step
                    if (heightAdjustment > 0.05f) {
                        std::cout << "\rClimbing to staircase: " << int((1.0f - portalProximity) * 100) << "% up, height: " 
                                  << (sideRoomHeight + heightAdjustment) << "        " << std::flush;
                        
                        // Add subtle screen shake for walking on stairs
                        if (movementVector.lengthSquared() > 0.0f) {
                            screenShakeAmount = std::max(screenShakeAmount, 0.05f);
                        }
                    }
                }
            }
        }
        
        // If player is in the staircase sector, create a stepped height effect
        if (currentSector == 4) {
            // Get player's position within the staircase
            float staircaseLength = 2.0f; // Increased length of the staircase to match new coordinates
            
            // Calculate how far along the staircase the player is (0 = start, 1 = end)
            // This is a simplified calculation - in a real game you'd use the actual path distance
            float progressAlongStaircase = (view.position.x - 1.0f) / staircaseLength;
            progressAlongStaircase = std::max(0.0f, std::min(1.0f, progressAlongStaircase));
            
            // Create 5 distinct steps with larger height changes
            int currentStep = static_cast<int>(progressAlongStaircase * 5);
            float stepHeight = 0.06f; // Slightly reduced height of each step for smoother movement
            
            if (!isJumping) {
                float staircaseBaseHeight = 0.2f;
                float stepElevation = currentStep * stepHeight;
                
                // Set player height based on current step - more dramatic change
                view.height = PLAYER_DEFAULT_HEIGHT + stepElevation;
                
                // Visual feedback for climbing the stairs
                std::cout << "\rOn staircase: Step " << (currentStep + 1) << " of 5, height: " 
                          << (staircaseBaseHeight + stepElevation) << "        " << std::flush;
                
                // Add screen shake when moving on stairs
                if (movementVector.lengthSquared() > 0.0f) {
                    // More shake on step transitions
                    float stepFraction = progressAlongStaircase * 5 - currentStep;
                    if (stepFraction < 0.2f || stepFraction > 0.8f) {
                        screenShakeAmount = std::max(screenShakeAmount, 0.12f); // Increased shake
                        
                        // Sound effect for stepping
                        if (rand() % 100 < 15) { // Increased chance of sound
                            std::cout << "SOUND EFFECT: *STEP*" << std::endl;
                        }
                    } else {
                        screenShakeAmount = std::max(screenShakeAmount, 0.05f);
                    }
                }
                
                // Add a visual indicator for the current step
                std::cout << "Current step: [";
                for (int i = 0; i < 5; i++) {
                    if (i == currentStep) {
                        std::cout << "X"; // Current step
                    } else if (i < currentStep) {
                        std::cout << "="; // Completed steps
                    } else {
                        std::cout << "-"; // Remaining steps
                    }
                }
                std::cout << "]" << std::endl;
            }
        }
        
        // If player is in the elevated room, provide visual feedback
        if (currentSector == 3) {
            // Check if player is near the portal to the staircase
            const Wall& portal = testMapSectors[3].walls[0]; // The portal wall in elevated room
            float distToPortal = portal.segment.distanceToPoint(view.position);
            
            if (distToPortal < 1.0f) {
                // Calculate how close the player is to the portal (0 = at portal, 1 = far from portal)
                float portalProximity = std::min(1.0f, distToPortal);
                
                // Adjust player height based on proximity to portal
                if (!isJumping) {
                    float staircaseTopHeight = 0.5f;
                    float elevatedRoomHeight = 0.5f;
                    float heightDifference = elevatedRoomHeight - staircaseTopHeight;
                    
                    // The closer to the portal, the lower the player should be (descending)
                    float heightAdjustment = heightDifference * portalProximity;
                    view.height = PLAYER_DEFAULT_HEIGHT + heightAdjustment;
                    
                    // Visual feedback for descending to the staircase
                    if (heightAdjustment < heightDifference - 0.05f) {
                        std::cout << "\rDescending to staircase: " << int(portalProximity * 100) << "% down        " << std::flush;
                        
                        // Add subtle screen shake for walking on stairs
                        if (movementVector.lengthSquared() > 0.0f) {
                            screenShakeAmount = std::max(screenShakeAmount, 0.05f);
                        }
                    }
                }
            } else {
                // When fully in the elevated room, provide clear visual feedback
                static int elevatedRoomTick = 0;
                elevatedRoomTick = (elevatedRoomTick + 1) % 60; // Cycle every ~1 second at 60fps
                
                if (elevatedRoomTick == 0) {
                    std::cout << "\r[ELEVATED ROOM] You are now at height 0.5 - Floor: Charred Bone - Ceiling: Pulsating Flesh" << std::flush;
                }
                
                // Add occasional ambient sounds for atmosphere
                if (rand() % 1000 < 5) {
                    std::string sounds[] = {
                        "*distant scream*",
                        "*bone cracking*",
                        "*flesh squelching*",
                        "*demonic whisper*",
                        "*wind howling*"
                    };
                    int soundIndex = rand() % 5;
                    std::cout << "\nAMBIENT SOUND: " << sounds[soundIndex] << std::endl;
                }
            }
        }
        
        // Check if player is near a portal
        bool nearPortal = false;
        for (int i = 0; i < testMapSectors.size(); i++) {
            for (int j = 0; j < testMapSectors[i].walls.size(); j++) {
                const Wall& wall = testMapSectors[i].walls[j];
                if (!wall.isSolid && wall.sectorBack >= 0) {
                    float dist = wall.segment.distanceToPoint(view.position);
                    if (dist < 1.0f) {
                        nearPortal = true;
                        // Only print once when getting near a portal
                        static float lastPortalDist = 999.0f;
                        if (lastPortalDist >= 1.0f) {
                            std::cout << "Near portal connecting sectors " << wall.sectorFront 
                                      << " and " << wall.sectorBack << " (distance: " << dist << ")" << std::endl;
                        }
                        lastPortalDist = dist;
                        break;
                    }
                }
            }
            if (nearPortal) break;
        }
        
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEMOTION && mouseControlEnabled) {
                // Apply mouse movement to camera rotation
                view.angle += event.motion.xrel * mouseSensitivity;
                
                // Normalize angle
                if (view.angle < 0) {
                    view.angle += 2 * M_PI;
                } else if (view.angle >= 2 * M_PI) {
                    view.angle -= 2 * M_PI;
                }
            } else if (event.type == SDL_MOUSEWHEEL && mouseControlEnabled) {
                // Mouse wheel for zooming or weapon selection
                if (event.wheel.y > 0) {
                    // Scroll up
                    std::cout << "Mouse wheel up - could be used for zooming in or switching to next weapon" << std::endl;
                } else if (event.wheel.y < 0) {
                    // Scroll down
                    std::cout << "Mouse wheel down - could be used for zooming out or switching to previous weapon" << std::endl;
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN && mouseControlEnabled) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Left mouse button - could be used for shooting or interaction
                    std::cout << "Left mouse button pressed - interaction" << std::endl;
                    
                    // Example: Print information about what's in front of the player
                    Vec2 rayDir(cos(view.angle), sin(view.angle));
                    Vec2 rayStart = view.position;
                    
                    // Cast a ray to find what's in front of the player
                    CollisionInfo hit = collisionBSP.castRay(rayStart, rayDir, 5.0f);
                    if (hit.collision) {
                        std::cout << "Interacting with wall at distance " << hit.distance << std::endl;
                        
                        // Get the wall that was hit
                        int sectorId = hit.sectorId;
                        int wallIndex = hit.wallIndex;
                        
                        if (sectorId >= 0 && sectorId < testMapSectors.size() && 
                            wallIndex >= 0 && wallIndex < testMapSectors[sectorId].walls.size()) {
                            const Wall& wall = testMapSectors[sectorId].walls[wallIndex];
                            std::cout << "Wall texture ID: " << wall.textureId << std::endl;
                            
                            // Display more detailed information about the wall
                            std::cout << "Wall from (" << wall.segment.start.position.x << ", " 
                                      << wall.segment.start.position.y << ") to (" 
                                      << wall.segment.end.position.x << ", " 
                                      << wall.segment.end.position.y << ")" << std::endl;
                                      
                            if (wall.isSolid) {
                                std::cout << "This is a solid wall" << std::endl;
                            } else {
                                std::cout << "This is a portal connecting sector " 
                                          << wall.sectorFront << " to sector " << wall.sectorBack << std::endl;
                            }
                        } else {
                            std::cout << "Invalid wall indices: sector=" << sectorId << ", wall=" << wallIndex << std::endl;
                        }
                    } else {
                        std::cout << "No wall hit within 5.0 units" << std::endl;
                    }
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    // Right mouse button - could be used for alt fire or secondary action
                    std::cout << "Right mouse button pressed - secondary action" << std::endl;
                }
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_w:
                        keyW = true;
                        break;
                    case SDLK_a:
                        keyA = true;
                        break;
                    case SDLK_s:
                        keyS = true;
                        break;
                    case SDLK_d:
                        keyD = true;
                        break;
                    case SDLK_q:
                        keyQ = true;
                        break;
                    case SDLK_e:
                        keyE = true;
                        break;
                    case SDLK_SPACE:
                        // Jump if on the ground
                        if (!isJumping && !isCrouching) {
                            isJumping = true;
                            verticalVelocity = JUMP_INITIAL_VELOCITY;
                            std::cout << "Player jumped!" << std::endl;
                            
                            // Sound effect simulation for jumping
                            std::cout << "\a"; // System beep as jump sound
                            std::cout << "SOUND EFFECT: *WHOOSH*" << std::endl;
                        }
                        keySpace = true;
                        break;
                    case SDLK_c:
                        // Toggle crouch
                        if (!isJumping) {
                            isCrouching = !isCrouching;
                            if (isCrouching) {
                                std::cout << "Player crouched" << std::endl;
                            } else {
                                std::cout << "Player stood up" << std::endl;
                            }
                        }
                        keyC = true;
                        break;
                    // Add a debug key to manually print wall information
                    case SDLK_p:
                        std::cout << "\n==== DEBUG WALL INFO ====\n";
                        std::cout << "Current position: (" << view.position.x << ", " 
                                  << view.position.y << ")\n";
                        std::cout << "Looking angle: " << view.angle << " radians\n";
                        std::cout << "Should be rendering walls at approximately:\n";
                        std::cout << "  - North wall at (x, 2.5) from x=-2.5 to x=2.5\n";
                        std::cout << "  - East wall at (2.5, y) from y=2.5 to y=-2.5\n";
                        std::cout << "  - South wall at (x, -2.5) from x=2.5 to x=-2.5\n";
                        std::cout << "  - West wall at (-2.5, y) from y=-2.5 to y=2.5\n";
                        std::cout << "=========================\n";
                        break;
                    case SDLK_l:
                        {
                            int sector = collisionBSP.findSector(view.position);
                            std::string sectorName;
                            switch (sector) {
                                case 0: sectorName = "Main Room"; break;
                                case 1: sectorName = "Corridor"; break;
                                case 2: sectorName = "Side Room"; break;
                                case 3: sectorName = "Elevated Room"; break;
                                case 4: sectorName = "Staircase"; break;
                                case 5: sectorName = "Hellish Pit"; break;
                                default: sectorName = "Unknown Sector"; break;
                            }
                            std::cout << "\n==== DEBUG SECTOR INFO ====\n";
                            std::cout << "Current position: (" << view.position.x << ", " 
                                      << view.position.y << ")\n";
                            std::cout << "Current sector: " << sectorName << " (ID: " << sector << ")\n";
                            
                            // Show floor height information
                            if (sector >= 0 && sector < testMapSectors.size()) {
                                std::cout << "Floor height: " << testMapSectors[sector].floorHeight << "\n";
                                std::cout << "Ceiling height: " << testMapSectors[sector].ceilingHeight << "\n";
                                std::cout << "Light level: " << testMapSectors[sector].lightLevel << "\n";
                            }
                            
                            // Find nearby portals
                            std::cout << "Nearby portals:\n";
                            for (int i = 0; i < testMapSectors.size(); i++) {
                                for (int j = 0; j < testMapSectors[i].walls.size(); j++) {
                                    const Wall& wall = testMapSectors[i].walls[j];
                                    if (!wall.isSolid && wall.sectorBack >= 0) {
                                        float dist = wall.segment.distanceToPoint(view.position);
                                        if (dist < 2.0f) {
                                            std::string fromSector, toSector;
                                            switch (wall.sectorFront) {
                                                case 0: fromSector = "Main Room"; break;
                                                case 1: fromSector = "Corridor"; break;
                                                case 2: fromSector = "Side Room"; break;
                                                case 3: fromSector = "Elevated Room"; break;
                                                case 4: fromSector = "Staircase"; break;
                                                case 5: fromSector = "Hellish Pit"; break;
                                                default: fromSector = "Unknown"; break;
                                            }
                                            switch (wall.sectorBack) {
                                                case 0: toSector = "Main Room"; break;
                                                case 1: toSector = "Corridor"; break;
                                                case 2: toSector = "Side Room"; break;
                                                case 3: toSector = "Elevated Room"; break;
                                                case 4: toSector = "Staircase"; break;
                                                case 5: toSector = "Hellish Pit"; break;
                                                default: toSector = "Unknown"; break;
                                            }
                                            std::cout << "  Portal at distance " << dist 
                                                      << " connecting " << fromSector << " to " << toSector << std::endl;
                                        }
                                    }
                                }
                            }
                            std::cout << "==========================\n";
                        }
                        break;
                    case SDLK_t: // Teleport to stairs for testing
                        view.position = Vec2(0.0f, -2.3f); // Position in front of stairs
                        view.angle = M_PI; // Face south (toward the stairs)
                        std::cout << "Teleported to stairs position." << std::endl;
                        break;
                    case SDLK_o: // Teleport to center platform for testing
                        view.position = Vec2(0.0f, 0.0f); // Position in center of room
                        view.angle = 0; // Face north
                        std::cout << "Teleported to center platform position." << std::endl;
                        break;
                    case SDLK_m:
                        // Toggle mouse control
                        mouseControlEnabled = !mouseControlEnabled;
                        SDL_SetRelativeMouseMode(mouseControlEnabled ? SDL_TRUE : SDL_FALSE);
                        std::cout << "Mouse control " << (mouseControlEnabled ? "enabled" : "disabled") << std::endl;
                        break;
                }
            } else if (event.type == SDL_KEYUP) {
                switch (event.key.keysym.sym) {
                    case SDLK_w:
                        keyW = false;
                        break;
                    case SDLK_a:
                        keyA = false;
                        break;
                    case SDLK_s:
                        keyS = false;
                        break;
                    case SDLK_d:
                        keyD = false;
                        break;
                    case SDLK_q:
                        keyQ = false;
                        break;
                    case SDLK_e:
                        keyE = false;
                        break;
                    case SDLK_SPACE:
                        keySpace = false;
                        break;
                    case SDLK_c:
                        keyC = false;
                        break;
                }
            }
        }
        
        // Update player height based on crouch state (with smooth transition)
        float targetHeight = isCrouching ? PLAYER_CROUCH_HEIGHT : PLAYER_DEFAULT_HEIGHT;
        if (!isJumping) {
            // Smoothly interpolate to target height when not jumping
            view.height = view.height + (targetHeight - view.height) * deltaTime * 5.0f;
        }
        
        // Apply jumping physics
        if (isJumping) {
            // Update height based on vertical velocity
            view.height += verticalVelocity;
            
            // Apply gravity
            verticalVelocity -= GRAVITY;
            
            // Check if landing
            if (view.height <= targetHeight) {
                // Calculate fall distance for screen shake
                float fallDistance = PLAYER_DEFAULT_HEIGHT - view.height + verticalVelocity;
                
                view.height = targetHeight;
                verticalVelocity = 0.0f;
                isJumping = false;
                std::cout << "Player landed" << std::endl;
                
                // Sound effect simulation for landing
                std::cout << "\a"; // System beep as landing sound
                std::cout << "SOUND EFFECT: *THUD*" << std::endl;
                
                // Add screen shake based on fall distance
                if (fallDistance > 0.05f) {
                    screenShakeAmount = fallDistance * 10.0f;
                    std::cout << "Screen shake: " << screenShakeAmount << std::endl;
                }
            }
            
            // Visual feedback for jumping
            std::cout << "\rJumping! Height: " << view.height << " Velocity: " << verticalVelocity << "        " << std::flush;
        } else if (isCrouching) {
            // Visual feedback for crouching
            std::cout << "\rCrouching! Height: " << view.height << "        " << std::flush;
        } else if (std::abs(view.height - PLAYER_DEFAULT_HEIGHT) < 0.01f) {
            // Clear status line when standing normally and fully upright
            std::cout << "\r                                                                " << std::flush;
        }
        
        // Apply collision detection and response if we're trying to move
        if (movementVector.lengthSquared() > 0.001f) {
            // Store original position for unstick detection
            Vec2 originalPosition = view.position;
            
            // Check if we're very close to a portal and trying to move through it
            bool portalAssist = false;
            for (int i = 0; i < testMapSectors.size() && !portalAssist; i++) {
                for (int j = 0; j < testMapSectors[i].walls.size(); j++) {
                    const Wall& wall = testMapSectors[i].walls[j];
                    if (!wall.isSolid && wall.sectorBack >= 0) {
                        float dist = wall.segment.distanceToPoint(view.position);
                        if (dist < 0.1f) { // Very close to portal
                            // Get wall direction and normal
                            Vec2 wallDir = (wall.segment.end.position - wall.segment.start.position).normalized();
                            Vec2 wallNormal(-wallDir.y, wallDir.x);
                            
                            // Check if we're trying to move through the portal
                            if (std::abs(movementVector.dotProduct(wallNormal)) > 0.05f) { // Reduced threshold to make it easier to cross
                                // If dot product of movement and normal is significant, we're trying to cross
                                // Add a stronger boost in the direction of the normal to help push through
                                float direction = movementVector.dotProduct(wallNormal) > 0 ? 1.0f : -1.0f;
                                Vec2 portalBoost = wallNormal * direction * 0.2f; // Increased from 0.15f to 0.2f
                                view.position = view.position + portalBoost;
                                
                                // Check if this is the portal between side room and elevated room or staircase
                                if ((wall.sectorFront == 2 && wall.sectorBack == 4) || // Side room to staircase
                                    (wall.sectorFront == 4 && wall.sectorBack == 2) || // Staircase to side room
                                    (wall.sectorFront == 4 && wall.sectorBack == 3) || // Staircase to elevated room
                                    (wall.sectorFront == 3 && wall.sectorBack == 4)) { // Elevated room to staircase
                                    // Apply an additional boost for elevation portals
                                    view.position = view.position + portalBoost * 3.0f;
                                    
                                    // Adjust height to match the destination sector's floor
                                    if (wall.sectorFront == 2 && wall.sectorBack == 4) {
                                        // Going from side room to staircase
                                        view.height = PLAYER_DEFAULT_HEIGHT + 0.2f; // Boost height to match staircase
                                        std::cout << "Enhanced portal assist applied for staircase!" << std::endl;
                                    } else if (wall.sectorFront == 4 && wall.sectorBack == 3) {
                                        // Going from staircase to elevated room
                                        view.height = PLAYER_DEFAULT_HEIGHT + 0.5f; // Boost height to match elevated room
                                        std::cout << "Enhanced portal assist applied for elevated room!" << std::endl;
                                    } else if (wall.sectorFront == 3 && wall.sectorBack == 4) {
                                        // Going from elevated room to staircase
                                        view.height = PLAYER_DEFAULT_HEIGHT + 0.4f; // Slightly lower for staircase top
                                        std::cout << "Enhanced portal assist applied for staircase from elevated room!" << std::endl;
                                    } else {
                                        // Going from staircase to side room
                                        view.height = PLAYER_DEFAULT_HEIGHT + 0.1f; // Reset height to match side room
                                        std::cout << "Enhanced portal assist applied for side room!" << std::endl;
                                    }
                                } else {
                                    std::cout << "Portal assist applied! Boosting player through portal." << std::endl;
                                }
                                
                                portalAssist = true;
                                break;
                            }
                        }
                    }
                }
            }
            
            // Check for nearby walls - debug output
            CollisionInfo nearbyWalls = collisionBSP.castRay(view.position, movementVector.normalized(), PLAYER_RADIUS * 3.0f);
            if (nearbyWalls.collision && nearbyWalls.distance < 0.5f) {
                // Only output when we're very close to a wall
                std::cout << "NEARBY WALL: Player at (" << view.position.x << ", " << view.position.y 
                          << "), Wall at " << nearbyWalls.distance * PLAYER_RADIUS * 3.0f 
                          << " units away in direction (" << movementVector.normalized().x 
                          << ", " << movementVector.normalized().y << ")" << std::endl;
            }
            
            // Check for collisions
            CollisionInfo collision = collisionBSP.checkCollision(view.position, PLAYER_RADIUS, movementVector);
            
            // Check if the player is standing on a platform
            int platformIndex = -1;
            bool onPlatform = collisionBSP.isPointOnPlatform(view.position, view.height - PLAYER_DEFAULT_HEIGHT, platformIndex);
            
            // If the player is on a platform, adjust their height
            if (onPlatform) {
                const Platform& platform = collisionBSP.getPlatforms()[platformIndex];
                
                // Set the player's height based on the platform height
                if (!isJumping) {
                    view.height = PLAYER_DEFAULT_HEIGHT + platform.getTopHeight();
                    
                    // If this is a stair platform, provide visual feedback
                    if (platform.type == PlatformType::STAIR) {
                        std::cout << "\rOn room-covering platform"
                                  << ", height: " << platform.getTopHeight() 
                                  << ", position: (" << view.position.x << ", " << view.position.y << ")"
                                  << "        " << std::flush;
                        
                        // Add subtle screen shake for walking on elevated surfaces
                        if (movementVector.lengthSquared() > 0.0f) {
                            screenShakeAmount = std::max(screenShakeAmount, 0.05f);
                        }
                    }
                }
            }
            
            // Debug: Check for platforms near the player
            float checkRadius = 1.0f;
            Vec2 playerPos = view.position;
            bool foundNearbyPlatform = false;
            
            // Check in a grid around the player
            for (float xOffset = -checkRadius; xOffset <= checkRadius; xOffset += 0.5f) {
                for (float yOffset = -checkRadius; yOffset <= checkRadius; yOffset += 0.5f) {
                    Vec2 checkPos = playerPos + Vec2(xOffset, yOffset);
                    int nearbyPlatformIndex = -1;
                    
                    if (collisionBSP.isPointOnPlatform(checkPos, view.height - PLAYER_DEFAULT_HEIGHT, nearbyPlatformIndex)) {
                        const Platform& nearbyPlatform = collisionBSP.getPlatforms()[nearbyPlatformIndex];
                        std::cout << "Nearby platform detected at (" << checkPos.x << ", " << checkPos.y 
                                  << "), type: " << (nearbyPlatform.type == PlatformType::STAIR ? "ELEVATED" : "OTHER")
                                  << ", height: " << nearbyPlatform.getTopHeight() << std::endl;
                        foundNearbyPlatform = true;
                    }
                }
            }
            
            if (!foundNearbyPlatform && std::abs(view.position.y + 2.3f) < 0.5f && std::abs(view.position.x) < 2.0f) {
                std::cout << "Player is near stairs location but no platform detected. Position: (" 
                          << view.position.x << ", " << view.position.y << ")" << std::endl;
            }
            
            // Check if the new position would be on a platform
            Vec2 newPosition = view.position + movementVector;
            int newPlatformIndex = -1;
            bool onNewPlatform = collisionBSP.isPointOnPlatform(newPosition, view.height - PLAYER_DEFAULT_HEIGHT, newPlatformIndex);
            
            // If moving from one platform to another, check height difference
            if (onPlatform && onNewPlatform && platformIndex != newPlatformIndex) {
                const Platform& currentPlatform = collisionBSP.getPlatforms()[platformIndex];
                const Platform& newPlatform = collisionBSP.getPlatforms()[newPlatformIndex];
                
                // Calculate height difference
                float heightDiff = newPlatform.getTopHeight() - currentPlatform.getTopHeight();
                
                // If the height difference is too great, prevent movement
                if (heightDiff > 0.3f && !isJumping) {
                    // Step is too high to climb normally
                    std::cout << "Step too high to climb. Height difference: " << heightDiff << std::endl;
                    collision.collision = true;
                    collision.distance = 0.0f;
                    collision.normal = (newPosition - view.position).normalized() * -1.0f;
                }
            }
            
            // Check if we're near the portal between side room and elevated room or staircase
            bool nearElevationPortal = false;
            for (int i = 0; i < testMapSectors.size() && !nearElevationPortal; i++) {
                for (int j = 0; j < testMapSectors[i].walls.size(); j++) {
                    const Wall& wall = testMapSectors[i].walls[j];
                    if (!wall.isSolid && 
                        ((wall.sectorFront == 2 && wall.sectorBack == 4) || // Side room to staircase
                         (wall.sectorFront == 4 && wall.sectorBack == 2) || // Staircase to side room
                         (wall.sectorFront == 4 && wall.sectorBack == 3) || // Staircase to elevated room
                         (wall.sectorFront == 3 && wall.sectorBack == 4))) { // Elevated room to staircase
                        float dist = wall.segment.distanceToPoint(view.position);
                        if (dist < 0.5f) { // Within 0.5 units of the elevation portal
                            nearElevationPortal = true;
                            
                            // Apply a small boost toward the portal to help player move through
                            Vec2 wallDir = (wall.segment.end.position - wall.segment.start.position).normalized();
                            Vec2 wallNormal(-wallDir.y, wallDir.x);
                            
                            // Determine which direction to boost (toward the portal)
                            float direction = 1.0f;
                            if (wall.sectorFront == currentSector) {
                                // We're in the front sector, so boost toward the back
                                direction = wallNormal.dotProduct(movementVector) > 0 ? 1.0f : -1.0f;
                            } else {
                                // We're in the back sector, so boost toward the front
                                direction = wallNormal.dotProduct(movementVector) > 0 ? -1.0f : 1.0f;
                            }
                            
                            // Apply a stronger boost for elevation changes
                            Vec2 portalBoost = wallNormal * direction * 0.2f;
                            view.position = view.position + portalBoost;
                            
                            std::cout << "Elevation portal assist applied! Boosting player through portal." << std::endl;
                            break;
                        }
                    }
                }
            }
            
            if (collision.collision) {
                // If we're near the elevation portal, be more lenient with collisions
                if (nearElevationPortal && collision.distance < 1.0f) {
                    // Reduce the collision effect for the elevation portal
                    collision.distance *= 1.5f; // Make it seem further away
                    std::cout << "Applying lenient collision detection near elevation portal" << std::endl;
                }
                
                // Output collision details when a collision is detected
                std::cout << "COLLISION: Distance=" << collision.distance 
                          << ", Normal=(" << collision.normal.x << ", " << collision.normal.y 
                          << "), SectorId=" << collision.sectorId
                          << ", WallIndex=" << collision.wallIndex << std::endl;
                
                // If we're about to hit a wall
                if (collision.distance < 1.0f) {
                    // Move as far as we can before hitting the wall
                    // Apply a small safety factor (0.9) to avoid getting too close
                    Vec2 safeMovement = movementVector * (collision.distance * 0.9f);
                    
                    // Move up to the collision point
                    view.position = view.position + safeMovement;
                    
                    // Calculate the remaining movement vector that needs to be redirected
                    Vec2 remainingMovement = movementVector * (1.0f - collision.distance * 0.9f);
                    
                    // Slide along the wall (project the remaining movement onto the wall plane)
                    Vec2 slideVector = remainingMovement - 
                                    collision.normal * remainingMovement.dotProduct(collision.normal);
                    
                    // Add a significant component away from the wall to prevent sticking
                    Vec2 awayFromWall = collision.normal * 0.01f;
                    slideVector = slideVector + awayFromWall;
                    
                    // Apply the slide movement, but check for a second collision
                    if (slideVector.lengthSquared() > 0.001f) {
                        CollisionInfo slideCollision = collisionBSP.checkCollision(view.position, PLAYER_RADIUS, slideVector);
                        
                        if (slideCollision.collision && slideCollision.distance < 1.0f) {
                            // If we'd hit another wall while sliding, move safely along the slide vector
                            // Reduce the sliding movement to avoid getting stuck in corners
                            float slideDistance = slideCollision.distance * 0.7f;
                            
                            // Add a stronger repulsion force to push away from corners
                            Vec2 repulsionForce = slideCollision.normal * 0.025f;
                            view.position = view.position + slideVector * slideDistance + repulsionForce;
                            
                            // If movement is very small, apply a larger bump in the normal direction to unstick
                            if (slideVector.length() * slideDistance < 0.015f) {
                                Vec2 unstickVector = collision.normal * 0.03f;
                                view.position = view.position + unstickVector;
                                
                            }
                        } else {
                            // No collision with the slide vector, apply it fully
                            view.position = view.position + slideVector;
                        }
                    }
                } else {
                    // Collision.distance >= 1.0 means no collision during this move
                    view.position = view.position + movementVector;
                }
            } else {
                // No collision, safe to move
                view.position = view.position + movementVector;
            }
            
            // Check if we've moved at all - if not, we might be stuck
            if ((view.position - originalPosition).lengthSquared() < 0.0001f) {
                // We haven't moved, so apply a larger random bump to unstick
                float randomAngle = static_cast<float>(rand()) / RAND_MAX * 2.0f * M_PI;
                Vec2 randomDir(std::cos(randomAngle), std::sin(randomAngle));
                view.position = view.position + randomDir * 0.05f;
                
                // Debug output for getting stuck
                std::cout << "MAJOR STUCK: Player at position (" << view.position.x << ", " << view.position.y 
                          << ") - applying stronger random bump in direction (" 
                          << randomDir.x << ", " << randomDir.y << ")" << std::endl;
                
                // Try another ray cast in the random direction to see what's there
                CollisionInfo stuckRay = collisionBSP.castRay(view.position, randomDir, PLAYER_RADIUS * 5.0f);
                if (stuckRay.collision) {
                    std::cout << "  Nearest obstacle in random direction at distance: " 
                              << stuckRay.distance * PLAYER_RADIUS * 5.0f << " units" << std::endl;
                }
            }
        }
        
        // Update view angle based on keyboard input
        if (keyQ) {
            view.angle -= turnSpeed;
            // Normalize angle
            if (view.angle < 0) {
                view.angle += 2 * M_PI;
            }
        }
        if (keyE) {
            view.angle += turnSpeed;
            // Normalize angle
            if (view.angle >= 2 * M_PI) {
                view.angle -= 2 * M_PI;
            }
        }
        
        // Before rendering the frame, apply screen shake
        // Apply screen shake if active
        ViewPosition shakingView = view;
        if (screenShakeAmount > 0.001f) {
            // Apply random offset to view position and height
            float shakeX = ((rand() % 1000) / 500.0f - 1.0f) * screenShakeAmount;
            float shakeY = ((rand() % 1000) / 500.0f - 1.0f) * screenShakeAmount;
            float shakeHeight = ((rand() % 1000) / 500.0f - 1.0f) * screenShakeAmount * 0.5f;
            
            shakingView.position.x += shakeX * 0.01f;
            shakingView.position.y += shakeY * 0.01f;
            shakingView.height += shakeHeight * 0.01f;
            
            // Decay the shake amount
            screenShakeAmount *= screenShakeDecay;
        }
        
        // Render frame using test map with the potentially shaking view
        std::vector<Sprite> emptySprites;
        
        // Debug: Print the number of platforms in the BSP tree
        const std::vector<Platform>& platforms = collisionBSP.getPlatforms();
        std::cout << "Rendering frame with " << platforms.size() << " platforms:" << std::endl;
        for (size_t i = 0; i < platforms.size(); i++) {
            const Platform& platform = platforms[i];
            std::cout << "  Platform " << i << ": type=" 
                      << (platform.type == PlatformType::STAIR ? "STAIR" : 
                         (platform.type == PlatformType::STATIC ? "STATIC" : "OTHER"))
                      << ", height=" << platform.height
                      << ", vertices=" << platform.vertices.size()
                      << ", position=(" << platform.vertices[0].x << "," << platform.vertices[0].y << ")"
                      << ", isVisible=" << (platform.isVisible ? "true" : "false")
                      << ", isSolid=" << (platform.isSolid ? "true" : "false")
                      << std::endl;
        }
        
        cudaRenderer.renderFrame(collisionBSP, shakingView, emptySprites, 0.016f);
        
        // Get the rendered frame back
        cudaRenderer.retrieveRenderingResults(frameBuffer, zBuffer);
        
        // Update the SDL texture with our frame buffer
        SDL_UpdateTexture(frameTexture, NULL, frameBuffer.data(), WIDTH * sizeof(Color));
        
        // Clear screen
        SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
        SDL_RenderClear(sdlRenderer);
        
        // Draw the texture
        SDL_RenderCopy(sdlRenderer, frameTexture, NULL, NULL);
        
        // Draw player state indicator
        SDL_Rect stateIndicator = {10, 10, 20, 20};
        if (isJumping) {
            // Red for jumping
            SDL_SetRenderDrawColor(sdlRenderer, 255, 50, 50, 255);
            SDL_RenderFillRect(sdlRenderer, &stateIndicator);
            
            // Draw jump height bar
            int jumpHeight = static_cast<int>((view.height - PLAYER_DEFAULT_HEIGHT) * 100);
            SDL_Rect jumpBar = {40, 10, 10, jumpHeight > 0 ? jumpHeight : 1};
            SDL_SetRenderDrawColor(sdlRenderer, 255, 150, 50, 255);
            SDL_RenderFillRect(sdlRenderer, &jumpBar);
            
            // Draw "JUMPING" label using small rectangles (pixel art style)
            // J
            SDL_Rect j1 = {70, 10, 3, 15};
            SDL_Rect j2 = {75, 10, 10, 3};
            SDL_Rect j3 = {75, 22, 10, 3};
            SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 255);
            SDL_RenderFillRect(sdlRenderer, &j1);
            SDL_RenderFillRect(sdlRenderer, &j2);
            SDL_RenderFillRect(sdlRenderer, &j3);
        } else if (isCrouching) {
            // Blue for crouching
            SDL_SetRenderDrawColor(sdlRenderer, 50, 50, 255, 255);
            SDL_RenderFillRect(sdlRenderer, &stateIndicator);
            
            // Draw "CROUCH" label using small rectangles (pixel art style)
            // C
            SDL_Rect c1 = {70, 10, 3, 15};
            SDL_Rect c2 = {73, 10, 10, 3};
            SDL_Rect c3 = {73, 22, 10, 3};
            SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 255);
            SDL_RenderFillRect(sdlRenderer, &c1);
            SDL_RenderFillRect(sdlRenderer, &c2);
            SDL_RenderFillRect(sdlRenderer, &c3);
        } else {
            // Green for normal
            SDL_SetRenderDrawColor(sdlRenderer, 50, 255, 50, 255);
            SDL_RenderFillRect(sdlRenderer, &stateIndicator);
        }
        
        // Draw height indicator
        int baseY = 50;
        int maxHeight = 100; // Maximum height of the indicator
        
        // Draw background bar
        SDL_Rect heightBarBg = {10, baseY, 20, maxHeight};
        SDL_SetRenderDrawColor(sdlRenderer, 50, 50, 50, 255);
        SDL_RenderFillRect(sdlRenderer, &heightBarBg);
        
        // Calculate height percentage based on current sector
        float heightPercentage = 0.0f;
        if (currentSector >= 0 && currentSector < testMapSectors.size()) {
            // Base the percentage on the floor height of the current sector
            float minHeight = 0.0f;  // Main room height
            float maxHeight = 0.5f;  // Elevated room height
            float currentHeight = testMapSectors[currentSector].floorHeight;
            
            // Normalize to 0-1 range
            heightPercentage = (currentHeight - minHeight) / (maxHeight - minHeight);
        }
        
        // Draw the filled portion of the height bar
        int filledHeight = static_cast<int>(heightPercentage * maxHeight);
        SDL_Rect heightBarFilled = {10, baseY + maxHeight - filledHeight, 20, filledHeight};
        
        // Color based on height (green for low, yellow for middle, red for high)
        if (heightPercentage < 0.33f) {
            SDL_SetRenderDrawColor(sdlRenderer, 50, 255, 50, 255); // Green
        } else if (heightPercentage < 0.66f) {
            SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 50, 255); // Yellow
        } else {
            SDL_SetRenderDrawColor(sdlRenderer, 255, 50, 50, 255); // Red
        }
        SDL_RenderFillRect(sdlRenderer, &heightBarFilled);
        
        // Draw height labels
        SDL_Rect labelHigh = {35, baseY, 5, 5};
        SDL_Rect labelMid = {35, baseY + maxHeight/2, 5, 5};
        SDL_Rect labelLow = {35, baseY + maxHeight - 5, 5, 5};
        SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 255, 255);
        SDL_RenderFillRect(sdlRenderer, &labelHigh);
        SDL_RenderFillRect(sdlRenderer, &labelMid);
        SDL_RenderFillRect(sdlRenderer, &labelLow);
        
        // Draw current sector indicator
        SDL_Rect sectorIndicator = {45, baseY + 10, 80, 15};
        SDL_SetRenderDrawColor(sdlRenderer, 30, 30, 30, 255);
        SDL_RenderFillRect(sdlRenderer, &sectorIndicator);
        
        // Draw a different colored rectangle based on current sector
        SDL_Rect sectorColor = {50, baseY + 15, 10, 5};
        switch (currentSector) {
            case 0: // Main Room
                SDL_SetRenderDrawColor(sdlRenderer, 100, 100, 255, 255); // Blue
                break;
            case 1: // Corridor
                SDL_SetRenderDrawColor(sdlRenderer, 100, 255, 100, 255); // Green
                break;
            case 2: // Side Room
                SDL_SetRenderDrawColor(sdlRenderer, 255, 255, 100, 255); // Yellow
                break;
            case 3: // Elevated Room
                SDL_SetRenderDrawColor(sdlRenderer, 255, 50, 50, 255); // Red
                break;
            case 4: // Staircase
                SDL_SetRenderDrawColor(sdlRenderer, 255, 150, 50, 255); // Orange
                break;
            case 5: // Hellish Pit
                SDL_SetRenderDrawColor(sdlRenderer, 100, 100, 100, 255); // Gray
                break;
            default:
                SDL_SetRenderDrawColor(sdlRenderer, 200, 200, 200, 255); // Gray
                break;
        }
        SDL_RenderFillRect(sdlRenderer, &sectorColor);
        
        // Render the minimap
        renderMinimap(sdlRenderer, testMapSectors, collisionBSP, Vec2(view.position.x, view.position.y), view.angle);
        
        // Present renderer
        SDL_RenderPresent(sdlRenderer);
        
        // Cap to ~60 FPS
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    // Clean up
    cudaRenderer.cleanup();
    SDL_DestroyTexture(frameTexture);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
} 