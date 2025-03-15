#ifndef TEXTURE_LOADER_H
#define TEXTURE_LOADER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <SDL.h>
#include <SDL_image.h>

namespace PureDoom {

// Forward declare Texture class
class Texture;
class Renderer;

class TextureLoader {
public:
    // Initialize the texture loader
    static bool initialize();
    
    // Clean up
    static void shutdown();
    
    // Load a texture from a file
    static std::shared_ptr<Texture> loadTexture(const std::string& filePath);
    
    // Get a texture by name (load if not already loaded)
    static std::shared_ptr<Texture> getTexture(const std::string& filePath);
    
    // Create a texture from an SDL_Surface
    static std::shared_ptr<Texture> createTextureFromSurface(SDL_Surface* surface);
    
    // Create a procedural texture
    static std::shared_ptr<Texture> createProceduralTexture(int width, int height, 
                                                           const std::string& type);
    
    // Create DOOM-style textures for the dungeon
    static void createDoomTextures();
    
    // Register DOOM-style textures with the renderer (ensures texture IDs match)
    static void registerDoomTexturesWithRenderer(Renderer* renderer);
    
private:
    // Map of loaded textures
    static std::unordered_map<std::string, std::shared_ptr<Texture>> s_textureCache;
    
    // Is the texture loader initialized?
    static bool s_initialized;
};

} // namespace PureDoom

#endif // TEXTURE_LOADER_H 