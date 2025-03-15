#include "TextureLoader.h"
#include "Renderer.h"
#include <iostream>
#include <filesystem>

namespace PureDoom {

// Static member initialization
std::unordered_map<std::string, std::shared_ptr<Texture>> TextureLoader::s_textureCache;
bool TextureLoader::s_initialized = false;

bool TextureLoader::initialize() {
    if (s_initialized) {
        return true;
    }
    
    // Initialize SDL_image
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        std::cerr << "SDL_image initialization failed: " << IMG_GetError() << std::endl;
        return false;
    }
    
    s_initialized = true;
    std::cout << "TextureLoader initialized successfully" << std::endl;
    
    // Create DOOM-style textures for our dungeon
    createDoomTextures();
    
    return true;
}

void TextureLoader::shutdown() {
    if (!s_initialized) {
        return;
    }
    
    // Clear texture cache
    s_textureCache.clear();
    
    // Quit SDL_image
    IMG_Quit();
    s_initialized = false;
    std::cout << "TextureLoader shut down" << std::endl;
}

std::shared_ptr<Texture> TextureLoader::loadTexture(const std::string& filePath) {
    if (!s_initialized) {
        std::cerr << "TextureLoader not initialized!" << std::endl;
        return nullptr;
    }
    
    // Check if texture is already loaded
    auto it = s_textureCache.find(filePath);
    if (it != s_textureCache.end()) {
        return it->second;
    }
    
    // Load the image using SDL_image
    SDL_Surface* surface = IMG_Load(filePath.c_str());
    if (!surface) {
        std::cerr << "Failed to load texture: " << filePath << " - " << IMG_GetError() << std::endl;
        return nullptr;
    }
    
    // Create texture from surface
    std::shared_ptr<Texture> texture = createTextureFromSurface(surface);
    
    // Free the surface
    SDL_FreeSurface(surface);
    
    if (texture) {
        // Store in cache
        s_textureCache[filePath] = texture;
        std::cout << "Loaded texture: " << filePath << " (" << texture->width() << "x" << texture->height() << ")" << std::endl;
    }
    
    return texture;
}

std::shared_ptr<Texture> TextureLoader::getTexture(const std::string& filePath) {
    // Check if the texture is already cached
    auto it = s_textureCache.find(filePath);
    if (it != s_textureCache.end()) {
        return it->second;
    }
    
    // Not found, try to load it
    return loadTexture(filePath);
}

std::shared_ptr<Texture> TextureLoader::createTextureFromSurface(SDL_Surface* surface) {
    if (!surface) {
        return nullptr;
    }
    
    // Get surface information
    int width = surface->w;
    int height = surface->h;
    
    // Create a new texture
    std::shared_ptr<Texture> texture = std::make_shared<Texture>(width, height);
    
    // Get pixel data
    SDL_LockSurface(surface);
    
    // Handle different surface formats
    uint8_t r, g, b, a;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Get pixel color based on surface format
            Uint32 pixel = 0;
            
            // Determine pixel location based on bytes per pixel
            int bpp = surface->format->BytesPerPixel;
            uint8_t* p = (uint8_t*)surface->pixels + y * surface->pitch + x * bpp;
            
            switch (bpp) {
                case 1: // 8-bit
                    pixel = *p;
                    break;
                case 2: // 16-bit
                    pixel = *(Uint16*)p;
                    break;
                case 3: // 24-bit
                    if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                        pixel = p[0] << 16 | p[1] << 8 | p[2];
                    } else {
                        pixel = p[0] | p[1] << 8 | p[2] << 16;
                    }
                    break;
                case 4: // 32-bit
                    pixel = *(Uint32*)p;
                    break;
            }
            
            // Get RGBA values
            SDL_GetRGBA(pixel, surface->format, &r, &g, &b, &a);
            
            // Set texture pixel
            texture->m_pixels[y * width + x] = Color(r, g, b, a);
        }
    }
    
    SDL_UnlockSurface(surface);
    
    return texture;
}

std::shared_ptr<Texture> TextureLoader::createProceduralTexture(int width, int height, const std::string& type) {
    std::shared_ptr<Texture> texture = std::make_shared<Texture>(width, height);
    
    // Clear texture data first
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            texture->m_pixels[y * width + x] = Color(0, 0, 0, 255);
        }
    }
    
    // DOOM DUNGEON TEXTURES
    if (type == "hell_brick") {
        // Generate hellish brick pattern with cracks and blood
        const int brickWidth = width / 4;
        const int brickHeight = height / 6;
        const int mortarSize = width / 48;
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Calculate brick row and column
                int brickRow = y / brickHeight;
                int brickCol = x / brickWidth;
                
                // Offset every other row by half a brick width
                if (brickRow % 2 == 1) {
                    brickCol = (x + brickWidth / 2) / brickWidth;
                }
                
                // Check if we're on a mortar line
                bool isMortar = (y % brickHeight < mortarSize) || 
                                (x % brickWidth < mortarSize) ||
                                ((brickRow % 2 == 1) && ((x + brickWidth / 2) % brickWidth < mortarSize));
                
                // Base brick color (dark red)
                int rBase = 120;
                int gBase = 30;
                int bBase = 30;
                
                // Add some variation to each brick
                int seed = (brickRow * 17 + brickCol * 23) % 100;
                int variation = seed % 30 - 15;
                
                // Mix in some darker bricks for variety
                if (seed > 70) {
                    rBase = 80;
                    gBase = 20;
                    bBase = 20;
                }
                
                // Add blood stains to some bricks
                bool hasBlood = (seed > 50 && seed < 65);
                if (hasBlood && !isMortar) {
                    // Blood pattern within the brick
                    int bloodX = x % brickWidth;
                    int bloodY = y % brickHeight;
                    int bloodDistance = std::abs(bloodX - brickWidth/2) + std::abs(bloodY - brickHeight/2);
                    
                    if (bloodDistance < brickWidth/3) {
                        rBase = 140;
                        gBase = 10;
                        bBase = 10;
                        
                        // Darker in the center of the blood stain
                        if (bloodDistance < brickWidth/6) {
                            rBase = 100;
                            gBase = 5;
                            bBase = 5;
                        }
                    }
                }
                
                // Add cracks to some bricks
                bool hasCrack = (seed > 30 && seed < 45);
                if (hasCrack && !isMortar) {
                    // Simple crack pattern
                    int crackX = x % brickWidth;
                    int crackY = y % brickHeight;
                    
                    // Create a wavy line through the brick
                    float angle = (float)brickRow / 10.0f;
                    int wavyY = brickHeight/2 + (int)(sin(crackX * 0.2f + angle) * brickHeight/4);
                    
                    if (abs(crackY - wavyY) < 2) {
                        rBase = rBase / 2;
                        gBase = gBase / 2;
                        bBase = bBase / 2;
                    }
                }
                
                // Set final color
                if (isMortar) {
                    texture->m_pixels[y * width + x] = Color(40, 40, 40); // Dark mortar
                } else {
                    texture->m_pixels[y * width + x] = Color(
                        std::min(255, std::max(0, rBase + variation)),
                        std::min(255, std::max(0, gBase + variation / 2)),
                        std::min(255, std::max(0, bBase + variation / 2))
                    );
                }
            }
        }
    }
    else if (type == "cracked_ceiling") {
        // Generate a cracked, dark ceiling texture
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Base dark ceiling color
                int baseR = 40;
                int baseG = 40;
                int baseB = 45;
                
                // Add some subtle noise
                int noise = ((x * 7 + y * 13) % 21) - 10;
                
                // Generate cracks through the ceiling
                bool isCrack = false;
                
                // Main cracks
                for (int i = 0; i < 5; i++) {
                    int seed = i * 1337 + 42;
                    int startX = seed % width;
                    int startY = (seed / width) % height;
                    
                    // Create a wavy crack
                    float angle = (float)i / 5.0f * 3.14159f;
                    int targetX = startX + (int)(cos(angle) * width * 0.7f);
                    int targetY = startY + (int)(sin(angle) * height * 0.7f);
                    
                    // Check if current pixel is on this crack
                    float t = 0.0f;
                    for (int step = 0; step < 30; step++) {
                        t = (float)step / 30.0f;
                        int crackX = startX + (int)((targetX - startX) * t);
                        int crackY = startY + (int)((targetY - startY) * t);
                        
                        // Add some waviness to the crack
                        crackX += (int)(sin(t * 30.0f) * 5.0f);
                        crackY += (int)(cos(t * 30.0f) * 5.0f);
                        
                        // Check if pixel is on or near the crack
                        if (abs(x - crackX) <= 1 && abs(y - crackY) <= 1) {
                            isCrack = true;
                            break;
                        }
                    }
                }
                
                // Set color based on whether it's a crack
                if (isCrack) {
                    texture->m_pixels[y * width + x] = Color(20, 20, 20); // Darker for cracks
                } else {
                    texture->m_pixels[y * width + x] = Color(
                        std::min(255, std::max(0, baseR + noise)),
                        std::min(255, std::max(0, baseG + noise)),
                        std::min(255, std::max(0, baseB + noise))
                    );
                }
            }
        }
    }
    else if (type == "pentagram_floor") {
        // Generate a dark stone floor with a pentagram
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Base dark stone color
                int baseR = 70;
                int baseG = 60;
                int baseB = 65;
                
                // Add some texture to the stone
                int noise = ((x * 11 + y * 17) % 31) - 15;
                
                // Normalize coordinates to -1 to 1 for drawing the pentagram
                float nx = (float)(x - width/2) / (width/2);
                float ny = (float)(y - height/2) / (height/2);
                float radius = sqrt(nx*nx + ny*ny);
                
                // Calculate angle in radians (0 to 2π)
                float angle = atan2(ny, nx);
                if (angle < 0) angle += 2 * M_PI;
                
                // Check if pixel is on the pentagram outline
                bool isOnPentagram = false;
                
                // Outer circle
                if (abs(radius - 0.8f) < 0.02f) {
                    isOnPentagram = true;
                }
                
                // Star points
                for (int i = 0; i < 5; i++) {
                    float pointAngle = i * 2 * M_PI / 5;
                    float nextAngle = ((i + 2) % 5) * 2 * M_PI / 5;
                    
                    // Line from center to point
                    float t = angle - pointAngle;
                    while (t < 0) t += 2 * M_PI;
                    while (t >= 2 * M_PI) t -= 2 * M_PI;
                    
                    if (t < 0.03f && radius < 0.8f) {
                        isOnPentagram = true;
                    }
                    
                    // Line between points
                    float ax = 0.8f * cos(pointAngle);
                    float ay = 0.8f * sin(pointAngle);
                    float bx = 0.8f * cos(nextAngle);
                    float by = 0.8f * sin(nextAngle);
                    
                    // Check if point is near the line
                    float dist = abs((by-ay)*nx - (bx-ax)*ny + bx*ay - by*ax) / 
                                sqrt((by-ay)*(by-ay) + (bx-ax)*(bx-ax));
                    if (dist < 0.02f) {
                        // Check if point is between endpoints
                        float dotproduct = (nx-ax)*(bx-ax) + (ny-ay)*(by-ay);
                        if (dotproduct >= 0 && dotproduct <= (bx-ax)*(bx-ax) + (by-ay)*(by-ay)) {
                            isOnPentagram = true;
                        }
                    }
                }
                
                // Set color
                if (isOnPentagram) {
                    // Blood red for the pentagram
                    texture->m_pixels[y * width + x] = Color(180, 20, 20);
                } else {
                    // Stone floor with some variation
                    texture->m_pixels[y * width + x] = Color(
                        std::min(255, std::max(0, baseR + noise)),
                        std::min(255, std::max(0, baseG + noise)),
                        std::min(255, std::max(0, baseB + noise))
                    );
                }
            }
        }
    }
    else if (type == "toxic_sludge") {
        // Generate glowing green toxic sludge
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Base toxic green color
                int baseR = 30;
                int baseG = 180;
                int baseB = 40;
                
                // Add undulating waves for a liquid effect
                float waveX = sin(x * 0.05f + y * 0.03f) * 20.0f;
                float waveY = cos(y * 0.04f + x * 0.02f) * 20.0f;
                
                // Brighten the peaks of the waves
                float brightness = (waveX + waveY) * 0.5f + 20.0f;
                
                // Add bubbles
                bool isBubble = false;
                for (int i = 0; i < 20; i++) {
                    // Random bubble positions
                    int seed = i * 927 + 31;
                    int bubbleX = seed % width;
                    int bubbleY = (seed / width) % height;
                    int bubbleSize = (seed % 5) + 2;
                    
                    // Check if point is within bubble
                    float dist = sqrt(pow(x - bubbleX, 2) + pow(y - bubbleY, 2));
                    if (dist < bubbleSize) {
                        isBubble = true;
                        break;
                    }
                }
                
                // Set color
                if (isBubble) {
                    // Lighter green for bubbles
                    texture->m_pixels[y * width + x] = Color(100, 255, 120);
                } else {
                    // Sludge with wave brightness
                    texture->m_pixels[y * width + x] = Color(
                        std::min(255, std::max(0, baseR + (int)brightness/2)),
                        std::min(255, std::max(0, baseG + (int)brightness)),
                        std::min(255, std::max(0, baseB + (int)brightness/3))
                    );
                }
            }
        }
    }
    else if (type == "blood_floor") {
        // Generate a floor with blood stains
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Base stone floor color
                int baseR = 80;
                int baseG = 70;
                int baseB = 75;
                
                // Add stone texture
                int noise = ((x * 13 + y * 19) % 31) - 15;
                
                // Blood stains
                bool hasBlood = false;
                float bloodIntensity = 0.0f;
                
                // Add several blood pools
                for (int i = 0; i < 8; i++) {
                    int seed = i * 541 + 123;
                    int bloodX = seed % width;
                    int bloodY = (seed / width) % height;
                    int bloodSize = (seed % 30) + 20;
                    
                    // Distance to blood center
                    float dist = sqrt(pow(x - bloodX, 2) + pow(y - bloodY, 2));
                    
                    // If within blood stain radius
                    if (dist < bloodSize) {
                        hasBlood = true;
                        // Fade out intensity at edges
                        float fade = 1.0f - (dist / bloodSize);
                        bloodIntensity = std::max(bloodIntensity, fade * fade);
                    }
                }
                
                // Add blood trails connecting some stains
                for (int i = 0; i < 5; i++) {
                    int seed = i * 317 + 211;
                    int startX = seed % width;
                    int startY = (seed / width) % height;
                    int endX = (seed * 13) % width;
                    int endY = ((seed * 13) / width) % height;
                    
                    // Check if point is near the blood trail
                    float dist = abs((endY-startY)*x - (endX-startX)*y + endX*startY - endY*startX) / 
                                sqrt(pow(endY-startY, 2) + pow(endX-startX, 2));
                    
                    // Width of trail varies
                    float trailWidth = 5.0f + (seed % 10);
                    
                    if (dist < trailWidth) {
                        // Check if point is between endpoints
                        float dotproduct = (x-startX)*(endX-startX) + (y-startY)*(endY-startY);
                        if (dotproduct >= 0 && dotproduct <= pow(endX-startX, 2) + pow(endY-startY, 2)) {
                            hasBlood = true;
                            // Fade based on distance from line center
                            float fade = 1.0f - (dist / trailWidth);
                            bloodIntensity = std::max(bloodIntensity, fade);
                        }
                    }
                }
                
                // Set color
                if (hasBlood) {
                    // Blend between stone and blood based on intensity
                    int r = baseR + noise + (int)((150 - baseR) * bloodIntensity);
                    int g = baseG + noise - (int)(baseG * 0.7f * bloodIntensity);
                    int b = baseB + noise - (int)(baseB * 0.7f * bloodIntensity);
                    
                    texture->m_pixels[y * width + x] = Color(
                        std::min(255, std::max(0, r)),
                        std::min(255, std::max(0, g)),
                        std::min(255, std::max(0, b))
                    );
                } else {
                    // Regular stone floor
                    texture->m_pixels[y * width + x] = Color(
                        std::min(255, std::max(0, baseR + noise)),
                        std::min(255, std::max(0, baseG + noise)),
                        std::min(255, std::max(0, baseB + noise))
                    );
                }
            }
        }
    }
    else if (type == "demonic_ceiling") {
        // Generate a ceiling with demonic symbols
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Base dark ceiling color
                int baseR = 50;
                int baseG = 40;
                int baseB = 45;
                
                // Add some texture
                int noise = ((x * 7 + y * 11) % 21) - 10;
                
                // Normalize coordinates for drawing symbols
                float nx = (float)(x % (width/2)) / (width/2) * 2.0f - 1.0f;
                float ny = (float)(y % (height/2)) / (height/2) * 2.0f - 1.0f;
                float radius = sqrt(nx*nx + ny*ny);
                
                // Various demonic symbols
                bool isOnSymbol = false;
                
                // Symbol 1: Pentagram (in each quadrant)
                float angle = atan2(ny, nx);
                if (angle < 0) angle += 2 * M_PI;
                
                if (abs(radius - 0.7f) < 0.03f) {
                    isOnSymbol = true;
                }
                
                // Lines of the pentagram
                for (int i = 0; i < 5; i++) {
                    float pointAngle = i * 2 * M_PI / 5;
                    float nextAngle = ((i + 2) % 5) * 2 * M_PI / 5;
                    
                    float ax = 0.7f * cos(pointAngle);
                    float ay = 0.7f * sin(pointAngle);
                    float bx = 0.7f * cos(nextAngle);
                    float by = 0.7f * sin(nextAngle);
                    
                    // Check if point is near the line
                    float dist = abs((by-ay)*nx - (bx-ax)*ny + bx*ay - by*ax) / 
                                sqrt((by-ay)*(by-ay) + (bx-ax)*(bx-ax));
                    if (dist < 0.03f) {
                        // Check if point is between endpoints
                        float dotproduct = (nx-ax)*(bx-ax) + (ny-ay)*(by-ay);
                        if (dotproduct >= 0 && dotproduct <= (bx-ax)*(bx-ax) + (by-ay)*(by-ay)) {
                            isOnSymbol = true;
                        }
                    }
                }
                
                // Symbol 2: Rune circles at center of each pentagram
                if (radius < 0.2f) {
                    if (abs(radius - 0.15f) < 0.02f) {
                        isOnSymbol = true;
                    }
                    
                    // Rune marks inside the circle
                    int runeCount = 8;
                    for (int i = 0; i < runeCount; i++) {
                        float runeAngle = i * 2 * M_PI / runeCount;
                        float rx = 0.15f * cos(runeAngle);
                        float ry = 0.15f * sin(runeAngle);
                        
                        if (abs(nx - rx) < 0.02f && abs(ny - ry) < 0.02f) {
                            isOnSymbol = true;
                        }
                    }
                }
                
                // Set color
                if (isOnSymbol) {
                    // Deep red for demonic symbols
                    texture->m_pixels[y * width + x] = Color(150, 10, 10);
                } else {
                    // Dark ceiling with texture
                    texture->m_pixels[y * width + x] = Color(
                        std::min(255, std::max(0, baseR + noise)),
                        std::min(255, std::max(0, baseG + noise)),
                        std::min(255, std::max(0, baseB + noise))
                    );
                }
            }
        }
    }
    else if (type == "dark_stone") {
        // Generate dark stone floor texture
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Base dark stone color
                int baseR = 50;
                int baseG = 50;
                int baseB = 55;
                
                // Add stone texture
                int large_noise = ((x / 4 * 13 + y / 4 * 17) % 41) - 20;
                int small_noise = ((x * 7 + y * 11) % 21) - 10;
                int noise = large_noise + small_noise / 2;
                
                // Add some darker spots randomly
                bool isDarker = ((x * 19 + y * 23) % 100) < 30;
                if (isDarker) {
                    baseR -= 15;
                    baseG -= 15;
                    baseB -= 15;
                }
                
                // Add some cracks
                bool isCrack = false;
                for (int i = 0; i < 12; i++) {
                    int seed = i * 237 + 51;
                    int startX = seed % width;
                    int startY = (seed / width) % height;
                    int endX = (seed * 7 + 111) % width;
                    int endY = ((seed * 7 + 111) / width) % height;
                    
                    // Check if point is near the crack
                    float dist = abs((endY-startY)*x - (endX-startX)*y + endX*startY - endY*startX) / 
                                sqrt(pow(endY-startY, 2) + pow(endX-startX, 2));
                    
                    if (dist < 1.0f) {
                        // Check if point is between endpoints
                        float dotproduct = (x-startX)*(endX-startX) + (y-startY)*(endY-startY);
                        if (dotproduct >= 0 && dotproduct <= pow(endX-startX, 2) + pow(endY-startY, 2)) {
                            isCrack = true;
                            break;
                        }
                    }
                }
                
                // Set color
                if (isCrack) {
                    // Darker for cracks
                    texture->m_pixels[y * width + x] = Color(30, 30, 35);
                } else {
                    // Stone with noise
                    texture->m_pixels[y * width + x] = Color(
                        std::min(255, std::max(0, baseR + noise)),
                        std::min(255, std::max(0, baseG + noise)),
                        std::min(255, std::max(0, baseB + noise))
                    );
                }
            }
        }
    }
    else if (type == "treasure_floor") {
        // Generate floor texture for secret area with treasure
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Base floor
                int baseR = 60;
                int baseG = 55;
                int baseB = 40;
                
                // Add texture
                int noise = ((x * 13 + y * 17) % 31) - 15;
                
                // Gold coins and treasure scattered on floor
                bool isTreasure = false;
                
                // Add gold coins
                for (int i = 0; i < 50; i++) {
                    int seed = i * 89 + 123;
                    int coinX = seed % width;
                    int coinY = (seed / width) % height;
                    int coinSize = (seed % 3) + 2;
                    
                    float dist = sqrt(pow(x - coinX, 2) + pow(y - coinY, 2));
                    if (dist < coinSize) {
                        isTreasure = true;
                        break;
                    }
                }
                
                // Add larger treasure items
                for (int i = 0; i < 5; i++) {
                    int seed = i * 157 + 321;
                    int itemX = seed % width;
                    int itemY = (seed / width) % height;
                    int itemSizeX = (seed % 10) + 5;
                    int itemSizeY = (seed % 8) + 4;
                    
                    if (abs(x - itemX) < itemSizeX && abs(y - itemY) < itemSizeY) {
                        isTreasure = true;
                        break;
                    }
                }
                
                // Set color
                if (isTreasure) {
                    // Gold color for treasure
                    texture->m_pixels[y * width + x] = Color(220 + noise/2, 180 + noise/2, 30);
                } else {
                    // Stone floor with texture
                    texture->m_pixels[y * width + x] = Color(
                        std::min(255, std::max(0, baseR + noise)),
                        std::min(255, std::max(0, baseG + noise)),
                        std::min(255, std::max(0, baseB + noise))
                    );
                }
            }
        }
    }
    else if (type == "stair_texture") {
        // Generate texture for stairs
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Base dark stone color
                int baseR = 60;
                int baseG = 55;
                int baseB = 50;
                
                // Add texture
                int noise = ((x * 11 + y * 13) % 21) - 10;
                
                // Create horizontal lines for stair steps
                const int stepHeight = height / 8;
                bool isStepEdge = (y % stepHeight < 2);
                
                if (isStepEdge) {
                    // Darker edge for each step
                    texture->m_pixels[y * width + x] = Color(40, 35, 30);
                } else {
                    // Stone texture
                    texture->m_pixels[y * width + x] = Color(
                        std::min(255, std::max(0, baseR + noise)),
                        std::min(255, std::max(0, baseG + noise)),
                        std::min(255, std::max(0, baseB + noise))
                    );
                }
            }
        }
    }
    else if (type == "hellish_ceiling") {
        // Generate a dark, hellish ceiling texture with embers
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Base dark ceiling
                int baseR = 30;
                int baseG = 20;
                int baseB = 25;
                
                // Add texture
                int noise = ((x * 7 + y * 11) % 21) - 10;
                
                // Add glowing embers/cracks
                bool isEmber = false;
                float emberIntensity = 0.0f;
                
                for (int i = 0; i < 30; i++) {
                    int seed = i * 173 + 71;
                    int emberX = seed % width;
                    int emberY = (seed / width) % height;
                    int emberSize = (seed % 5) + 2;
                    
                    // Distance to ember center
                    float dist = sqrt(pow(x - emberX, 2) + pow(y - emberY, 2));
                    
                    // If within ember radius
                    if (dist < emberSize) {
                        isEmber = true;
                        // Fade out intensity at edges
                        float fade = 1.0f - (dist / emberSize);
                        emberIntensity = std::max(emberIntensity, fade * fade);
                    }
                }
                
                // Add some cracks with embers
                for (int i = 0; i < 10; i++) {
                    int seed = i * 257 + 37;
                    int startX = seed % width;
                    int startY = (seed / width) % height;
                    int endX = (seed * 11 + 93) % width;
                    int endY = ((seed * 11 + 93) / width) % height;
                    
                    // Check if point is near the crack
                    float dist = abs((endY-startY)*x - (endX-startX)*y + endX*startY - endY*startX) / 
                                sqrt(pow(endY-startY, 2) + pow(endX-startX, 2));
                    
                    if (dist < 1.5f) {
                        // Check if point is between endpoints
                        float dotproduct = (x-startX)*(endX-startX) + (y-startY)*(endY-startY);
                        if (dotproduct >= 0 && dotproduct <= pow(endX-startX, 2) + pow(endY-startY, 2)) {
                            isEmber = true;
                            emberIntensity = std::max(emberIntensity, 1.0f - (dist / 1.5f));
                        }
                    }
                }
                
                // Set color
                if (isEmber) {
                    // Glowing orange-red for embers
                    int r = baseR + noise + (int)((200 - baseR) * emberIntensity);
                    int g = baseG + noise + (int)((100 - baseG) * emberIntensity);
                    int b = baseB + noise + (int)((20 - baseB) * emberIntensity);
                    
                    texture->m_pixels[y * width + x] = Color(
                        std::min(255, std::max(0, r)),
                        std::min(255, std::max(0, g)),
                        std::min(255, std::max(0, b))
                    );
                } else {
                    // Dark ceiling texture
                    texture->m_pixels[y * width + x] = Color(
                        std::min(255, std::max(0, baseR + noise)),
                        std::min(255, std::max(0, baseG + noise)),
                        std::min(255, std::max(0, baseB + noise))
                    );
                }
            }
        }
    }
    else if (type == "pipe_ceiling") {
        // Generate ceiling texture with pipes
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Base ceiling color
                int baseR = 45;
                int baseG = 40;
                int baseB = 35;
                
                // Add texture
                int noise = ((x * 7 + y * 13) % 21) - 10;
                
                // Create pipes in a grid pattern
                bool isPipe = false;
                bool isPipeEdge = false;
                
                // Horizontal pipes
                const int pipeSpacing = height / 5;
                const int pipeWidth = height / 15;
                for (int i = 0; i < 5; i++) {
                    int pipeY = i * pipeSpacing + pipeSpacing / 2;
                    if (abs(y - pipeY) < pipeWidth) {
                        isPipe = true;
                        if (abs(y - pipeY) > pipeWidth - 2) {
                            isPipeEdge = true;
                        }
                    }
                }
                
                // Vertical pipes
                for (int i = 0; i < 5; i++) {
                    int pipeX = i * (width / 5) + (width / 10);
                    if (abs(x - pipeX) < pipeWidth) {
                        isPipe = true;
                        if (abs(x - pipeX) > pipeWidth - 2) {
                            isPipeEdge = true;
                        }
                    }
                }
                
                // Pipe connectors (joints)
                for (int i = 0; i < 5; i++) {
                    for (int j = 0; j < 5; j++) {
                        int jointX = j * (width / 5) + (width / 10);
                        int jointY = i * pipeSpacing + pipeSpacing / 2;
                        
                        float dist = sqrt(pow(x - jointX, 2) + pow(y - jointY, 2));
                        if (dist < pipeWidth * 1.2f) {
                            isPipe = true;
                            if (dist > pipeWidth * 1.2f - 2) {
                                isPipeEdge = true;
                            }
                        }
                    }
                }
                
                // Set color
                if (isPipe) {
                    if (isPipeEdge) {
                        // Darker edge for pipes
                        texture->m_pixels[y * width + x] = Color(30, 28, 25);
                    } else {
                        // Metallic pipe color
                        texture->m_pixels[y * width + x] = Color(80 + noise, 75 + noise, 70 + noise);
                    }
                } else {
                    // Ceiling base
                    texture->m_pixels[y * width + x] = Color(
                        std::min(255, std::max(0, baseR + noise)),
                        std::min(255, std::max(0, baseG + noise)),
                        std::min(255, std::max(0, baseB + noise))
                    );
                }
            }
        }
    }
    else if (type == "checkerboard") {
        // Generate checkerboard pattern
        const int tileSize = width / 8;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                bool isEvenTileX = ((x / tileSize) % 2) == 0;
                bool isEvenTileY = ((y / tileSize) % 2) == 0;
                bool isLight = isEvenTileX != isEvenTileY;
                
                Color color = isLight ? Color(200, 200, 200) : Color(70, 70, 70);
                
                // Add a border
                if (x % tileSize == 0 || y % tileSize == 0) {
                    color = Color(120, 120, 120);
                }
                
                texture->m_pixels[y * width + x] = color;
            }
        }
    }
    else if (type == "brick") {
        // Generate brick pattern
        const int brickWidth = width / 4;
        const int brickHeight = height / 8;
        const int mortarSize = width / 64;
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Calculate brick row and column
                int brickRow = y / brickHeight;
                int brickCol = x / brickWidth;
                
                // Offset every other row by half a brick width
                if (brickRow % 2 == 1) {
                    brickCol = (x + brickWidth / 2) / brickWidth;
                }
                
                // Check if we're on a mortar line
                bool isMortar = (y % brickHeight < mortarSize) || 
                                (x % brickWidth < mortarSize) ||
                                ((brickRow % 2 == 1) && ((x + brickWidth / 2) % brickWidth < mortarSize));
                
                // Set color
                if (isMortar) {
                    texture->m_pixels[y * width + x] = Color(128, 128, 128); // Gray mortar
                } else {
                    // Vary the brick color slightly
                    int variation = ((brickRow * 13 + brickCol * 17) % 32) - 16;
                    texture->m_pixels[y * width + x] = Color(170 + variation, 60 + variation, 50 + variation);
                }
            }
        }
    }
    else if (type == "gradient") {
        // Generate vertical gradient
        for (int y = 0; y < height; y++) {
            float t = static_cast<float>(y) / height;
            Color color = Color::fromHSV(t * 360.0f, 0.8f, 0.9f);
            
            for (int x = 0; x < width; x++) {
                texture->m_pixels[y * width + x] = color;
            }
        }
    }
    else if (type == "wall1" || type == "stone") {
        // Generate stone wall texture
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Create base stone color
                int noise = ((x * 23 + y * 37) % 31) - 15;
                Color stoneColor = Color(100 + noise, 100 + noise, 100 + noise);
                
                // Add some variation - cracks and bumps
                int patternX = x % (width / 4);
                int patternY = y % (height / 4);
                
                // Create cracks
                if ((patternX == 0 || patternY == 0) && ((x + y) % 5 == 0)) {
                    stoneColor = Color(60, 60, 60);
                }
                
                // Add some moss/dirt
                if ((x + y) % 17 == 0) {
                    stoneColor = Color(80, 100 + noise, 60);
                }
                
                texture->m_pixels[y * width + x] = stoneColor;
            }
        }
    }
    else if (type == "floor1" || type == "tile") {
        // Generate tile floor texture
        const int tileSize = width / 6;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Base color
                Color color;
                
                // Create tiles
                int tileX = x / tileSize;
                int tileY = y / tileSize;
                bool isEvenTile = (tileX + tileY) % 2 == 0;
                
                if (isEvenTile) {
                    color = Color(200, 180, 150); // Light tile
                } else {
                    color = Color(160, 140, 120); // Dark tile
                }
                
                // Add some noise/texture
                int noise = ((x * 7 + y * 13) % 21) - 10;
                color.r = std::max(0, std::min(255, static_cast<int>(color.r) + noise));
                color.g = std::max(0, std::min(255, static_cast<int>(color.g) + noise));
                color.b = std::max(0, std::min(255, static_cast<int>(color.b) + noise));
                
                // Add grout lines
                if (x % tileSize <= 1 || y % tileSize <= 1) {
                    color = Color(100, 100, 100);
                }
                
                texture->m_pixels[y * width + x] = color;
            }
        }
    }
    else if (type == "ceiling1" || type == "panel") {
        // Generate ceiling panel texture
        const int panelWidth = width / 3;
        const int panelHeight = height / 3;
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // Base color
                Color color = Color(220, 220, 225); // Light ceiling color
                
                // Create panel effect
                int panelX = x % panelWidth;
                int panelY = y % panelHeight;
                
                // Panel borders
                bool isBorder = (panelX < 2 || panelX >= panelWidth - 2 || 
                                 panelY < 2 || panelY >= panelHeight - 2);
                
                // Panel center
                bool isCenter = (panelX > panelWidth/4 && panelX < 3*panelWidth/4 &&
                                 panelY > panelHeight/4 && panelY < 3*panelHeight/4);
                
                if (isBorder) {
                    color = Color(180, 180, 185); // Darker color for border
                } else if (isCenter) {
                    color = Color(230, 230, 235); // Lighter color for center
                }
                
                // Add some subtle noise
                int noise = ((x * 7 + y * 13) % 21) - 10;
                color.r = std::min(255, std::max(0, static_cast<int>(color.r) + noise));
                color.g = std::min(255, std::max(0, static_cast<int>(color.g) + noise));
                color.b = std::min(255, std::max(0, static_cast<int>(color.b) + noise));
                
                texture->m_pixels[y * width + x] = color;
            }
        }
    }
    else if (type == "sprite_blue" || type == "blue_orb") {
        // Create a blue orb sprite
        int centerX = width / 2;
        int centerY = height / 2;
        float radius = width * 0.4f;
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float dx = x - centerX;
                float dy = y - centerY;
                float distance = std::sqrt(dx * dx + dy * dy);
                
                if (distance < radius) {
                    // Inside circle - create blue orb effect
                    float factor = distance / radius;
                    Color color = Color::fromHSV(240.0f, 0.8f - factor * 0.3f, 1.0f - factor * 0.5f);
                    
                    // Add highlight
                    if (dx > -radius/3 && dx < 0 && dy > -radius/3 && dy < 0) {
                        float highlightDist = std::sqrt(dx*dx + dy*dy) / (radius/3);
                        if (highlightDist < 1.0f) {
                            float highlight = 1.0f - highlightDist;
                            color.r = std::min(255, color.r + static_cast<int>(highlight * 100));
                            color.g = std::min(255, color.g + static_cast<int>(highlight * 100));
                            color.b = std::min(255, color.b + static_cast<int>(highlight * 100));
                        }
                    }
                    
                    texture->m_pixels[y * width + x] = color;
                } else {
                    // Outside circle - transparent
                    texture->m_pixels[y * width + x] = Color(0, 0, 0, 0);
                }
            }
        }
    }
    else if (type == "sprite_green" || type == "green_gem") {
        // Create a green gem sprite
        int centerX = width / 2;
        int centerY = height / 2;
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float dx = (x - centerX) / (float)(width / 2);
                float dy = (y - centerY) / (float)(height / 2);
                
                // Diamond shape
                float distance = std::abs(dx) + std::abs(dy);
                
                if (distance < 0.8f) {
                    // Inside diamond
                    float factor = distance / 0.8f;
                    Color color = Color::fromHSV(120.0f, 0.9f - factor * 0.2f, 0.9f - factor * 0.3f);
                    
                    // Add light refraction effect
                    float angle = std::atan2(dy, dx) + PI;
                    float refraction = (std::sin(angle * 5) + 1.0f) * 0.5f;
                    color.r = std::min(255, color.r + static_cast<int>(refraction * 40));
                    color.g = std::min(255, color.g + static_cast<int>(refraction * 40));
                    color.b = std::min(255, color.b + static_cast<int>(refraction * 40));
                    
                    texture->m_pixels[y * width + x] = color;
                } else {
                    // Outside diamond - transparent
                    texture->m_pixels[y * width + x] = Color(0, 0, 0, 0);
                }
            }
        }
    }
    else if (type == "sprite_red" || type == "red_enemy") {
        // Create a red enemy sprite
        int centerX = width / 2;
        int centerY = height / 2;
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float dx = (x - centerX) / (float)(width / 2);
                float dy = (y - centerY) / (float)(height / 2);
                
                // Create an interesting shape - combination of diamond and circle
                float diamondDist = std::abs(dx) + std::abs(dy);
                float circleDist = std::sqrt(dx*dx + dy*dy) * 1.2f;
                float distance = std::min(diamondDist, circleDist);
                
                if (distance < 0.9f) {
                    // Inside shape
                    float factor = distance / 0.9f;
                    Color color = Color::fromHSV(0.0f, 0.9f, 1.0f - factor * 0.5f);
                    
                    // Add details
                    if (std::abs(dy) < 0.2f && dx > 0.1f) {
                        // Eye
                        color = Color(255, 255, 200);
                        if (std::abs(dy) < 0.1f && dx > 0.3f) {
                            color = Color(0, 0, 0);
                        }
                    }
                    
                    texture->m_pixels[y * width + x] = color;
                } else {
                    // Outside shape - transparent
                    texture->m_pixels[y * width + x] = Color(0, 0, 0, 0);
                }
            }
        }
    }
    else {
        // Default to checkerboard if type isn't recognized
        std::cout << "Unrecognized procedural texture type: " << type << ", defaulting to checkerboard" << std::endl;
        return createProceduralTexture(width, height, "checkerboard");
    }
    
    // Store in cache with a special name
    std::string cacheName = "procedural_" + type + "_" + std::to_string(width) + "x" + std::to_string(height);
    s_textureCache[cacheName] = texture;
    
    return texture;
}

// Create DOOM-style textures for the dungeon (with IDs 101-133)
void TextureLoader::createDoomTextures() {
    // Basic texture size
    const int texSize = 256;
    
    // Register textures for our DOOM-style dungeon (101-133)
    
    // 101: Dark stone floor (entrance hall floor)
    auto darkStoneTexture = createProceduralTexture(texSize, texSize, "dark_stone");
    s_textureCache["doom_tex_101"] = darkStoneTexture;
    
    // 102: Cracked ceiling (entrance hall ceiling)
    auto crackedCeilingTexture = createProceduralTexture(texSize, texSize, "cracked_ceiling");
    s_textureCache["doom_tex_102"] = crackedCeilingTexture;
    
    // 103-106: Hell brick walls (entrance hall walls)
    for (int i = 103; i <= 106; ++i) {
        auto hellBrickTexture = createProceduralTexture(texSize, texSize, "hell_brick");
        s_textureCache["doom_tex_" + std::to_string(i)] = hellBrickTexture;
    }
    
    // 107: Stair texture
    auto stairTexture = createProceduralTexture(texSize, texSize, "stair_texture");
    s_textureCache["doom_tex_107"] = stairTexture;
    
    // 108-110: More wall textures for stairway
    for (int i = 108; i <= 110; ++i) {
        auto hellBrickTexture = createProceduralTexture(texSize, texSize, "hell_brick");
        s_textureCache["doom_tex_" + std::to_string(i)] = hellBrickTexture;
    }
    
    // 111: Pentagram floor (main chamber floor)
    auto pentagramTexture = createProceduralTexture(texSize, texSize, "pentagram_floor");
    s_textureCache["doom_tex_111"] = pentagramTexture;
    
    // 112: Hellish ceiling (main chamber ceiling)
    auto hellishCeilingTexture = createProceduralTexture(texSize, texSize, "hellish_ceiling");
    s_textureCache["doom_tex_112"] = hellishCeilingTexture;
    
    // 113-117: More wall textures for main chamber
    for (int i = 113; i <= 117; ++i) {
        auto hellBrickTexture = createProceduralTexture(texSize, texSize, "hell_brick");
        s_textureCache["doom_tex_" + std::to_string(i)] = hellBrickTexture;
    }
    
    // 118: Toxic sludge (sludge room floor)
    auto toxicSludgeTexture = createProceduralTexture(texSize, texSize, "toxic_sludge");
    s_textureCache["doom_tex_118"] = toxicSludgeTexture;
    
    // 119: Pipe ceiling (sludge room ceiling)
    auto pipeCeilingTexture = createProceduralTexture(texSize, texSize, "pipe_ceiling");
    s_textureCache["doom_tex_119"] = pipeCeilingTexture;
    
    // 120-122: Wall textures for sludge room
    for (int i = 120; i <= 122; ++i) {
        auto hellBrickTexture = createProceduralTexture(texSize, texSize, "hell_brick");
        s_textureCache["doom_tex_" + std::to_string(i)] = hellBrickTexture;
    }
    
    // 123: Blood floor (ritual chamber floor)
    auto bloodFloorTexture = createProceduralTexture(texSize, texSize, "blood_floor");
    s_textureCache["doom_tex_123"] = bloodFloorTexture;
    
    // 124: Demonic ceiling (ritual chamber ceiling)
    auto demonicCeilingTexture = createProceduralTexture(texSize, texSize, "demonic_ceiling");
    s_textureCache["doom_tex_124"] = demonicCeilingTexture;
    
    // 125-127: Wall textures for ritual chamber
    for (int i = 125; i <= 127; ++i) {
        auto hellBrickTexture = createProceduralTexture(texSize, texSize, "hell_brick");
        s_textureCache["doom_tex_" + std::to_string(i)] = hellBrickTexture;
    }
    
    // 128: Treasure floor (secret area floor)
    auto treasureFloorTexture = createProceduralTexture(texSize, texSize, "treasure_floor");
    s_textureCache["doom_tex_128"] = treasureFloorTexture;
    
    // 129: Low detailed ceiling (secret area ceiling)
    auto lowCeilingTexture = createProceduralTexture(texSize, texSize, "cracked_ceiling");
    s_textureCache["doom_tex_129"] = lowCeilingTexture;
    
    // 130-133: Wall textures for secret area
    for (int i = 130; i <= 133; ++i) {
        auto hellBrickTexture = createProceduralTexture(texSize, texSize, "hell_brick");
        s_textureCache["doom_tex_" + std::to_string(i)] = hellBrickTexture;
    }
    
    std::cout << "Created " << 33 << " DOOM-style textures for the dungeon" << std::endl;
}

// Register DOOM-style textures with the renderer to ensure texture IDs match
void TextureLoader::registerDoomTexturesWithRenderer(Renderer* renderer) {
    if (!renderer) {
        std::cerr << "Cannot register DOOM textures with null renderer" << std::endl;
        return;
    }
    
    std::cout << "Registering DOOM textures with renderer..." << std::endl;
    
    // First, determine how many base textures are in the renderer
    // We need to ensure our DOOM textures start at index 101
    size_t baseTextureCount = 101;
    
    // Add placeholder textures if needed to ensure our IDs start at 101
    std::shared_ptr<Texture> placeholderTexture = createProceduralTexture(64, 64, "checkerboard");
    for (size_t i = 0; i < baseTextureCount; i++) {
        renderer->addTexture(*placeholderTexture);
    }
    
    // Now register all the DOOM dungeon textures with their correct IDs
    for (int i = 101; i <= 133; i++) {
        std::string textureName = "doom_tex_" + std::to_string(i);
        auto it = s_textureCache.find(textureName);
        
        if (it != s_textureCache.end()) {
            // Add the texture to the renderer at the correct index
            renderer->addTexture(*(it->second));
            std::cout << "Registered DOOM texture ID " << i << " with renderer" << std::endl;
        } else {
            // Texture not found, add placeholder instead
            std::cerr << "Warning: DOOM texture ID " << i << " not found in cache, using placeholder" << std::endl;
            renderer->addTexture(*placeholderTexture);
        }
    }
    
    std::cout << "Finished registering DOOM textures with renderer" << std::endl;
}

} // namespace PureDoom 