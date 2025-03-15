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
    
    if (type == "checkerboard") {
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
                    color = Color(180, 180, 190); // Darker border
                } else if (isCenter) {
                    color = Color(240, 240, 245); // Lighter center
                }
                
                // Add subtle noise
                int noise = ((x * 11 + y * 19) % 15) - 7;
                color.r = std::max(0, std::min(255, static_cast<int>(color.r) + noise));
                color.g = std::max(0, std::min(255, static_cast<int>(color.g) + noise));
                color.b = std::max(0, std::min(255, static_cast<int>(color.b) + noise));
                
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

} // namespace PureDoom 