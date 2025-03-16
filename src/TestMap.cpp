#include "RendererCuda.h"
#include "Renderer.h"
#include "BSPTree.h"
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
        "CUDA Test Map",
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
    
    // Create frame buffer and z-buffer
    std::vector<Color> frameBuffer(WIDTH * HEIGHT, Color(0, 0, 0));
    std::vector<float> zBuffer(WIDTH * HEIGHT, 1.0f);
    
    // Create textures for our test map
    std::vector<Texture> textures;
    
    // 0: Floor texture (DOOM-like floor pattern)
    textures.push_back(createDoomFloorTexture(64, 64));
    
    // 1: Ceiling texture
    textures.push_back(createSimpleTexture(64, 64, 60, 60, 80));
    
    // 2: Side room floor texture (DOOM-like reddish floor)
    Texture sideRoomFloor = createDoomFloorTexture(64, 64);
    // Tint it redder
    for (int i = 0; i < sideRoomFloor.width() * sideRoomFloor.height(); i++) {
        Color& pixel = sideRoomFloor.m_pixels[i];
        pixel.r = std::min(255, pixel.r + 40);
        pixel.g = std::max(0, pixel.g - 10);
        pixel.b = std::max(0, pixel.b - 10);
    }
    textures.push_back(sideRoomFloor);
    
    // 3: Main room wall texture (brick)
    textures.push_back(createBrickTexture(128, 128, Color(180, 100, 80), Color(100, 100, 100)));
    
    // 4: Portal texture (blue-ish)
    textures.push_back(createSimpleTexture(64, 64, 30, 30, 150));
    
    // 5: Corridor wall texture (stone)
    textures.push_back(createCheckerboardTexture(64, 64, Color(120, 120, 140), Color(100, 100, 120), 4));
    
    // 6: Side room wall texture (wood)
    textures.push_back(createSimpleTexture(64, 64, 120, 80, 40));
    
    // Upload textures to CUDA
    cudaRenderer.uploadTextures(textures);
    
    // Debug output for texture upload status
    std::cout << "--------- DEBUG TEXTURE INFORMATION ---------" << std::endl;
    std::cout << "Texture upload successful: " << (cudaRenderer.areTexturesUploaded() ? "YES" : "NO") << std::endl;
    std::cout << "Number of textures created: " << textures.size() << std::endl;
    std::cout << "Number of textures uploaded: " << cudaRenderer.getNumTextures() << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
    
    // Configure skybox
    Skybox skybox;
    skybox.zenithColor = Color(40, 40, 100);
    skybox.horizonColor = Color(100, 100, 150);
    skybox.maxViewDistance = 30.0f;
    skybox.dynamicSky = true;
    skybox.sunAngle = 1.0f;  // Position in radians
    skybox.sunHeight = 0.3f; // 0.0 = horizon, 1.0 = zenith
    skybox.sunSize = 0.02f;  // Relative size
    skybox.sunColor = Color(255, 240, 200);
    skybox.sunGlowColor = Color(255, 180, 100);
    skybox.sunGlowSize = 5.0f;
    cudaRenderer.setSkybox(skybox);
    
    // Initialize view position near the center of main room
    ViewPosition view;
    view.position.x = 0.0f;
    view.position.y = 0.0f;
    view.height = 0.8f;
    view.angle = 0.0f;  // Facing north
    view.fov = 90.0f;   // Field of view in degrees
    
    // Enable test map mode
    cudaRenderer.useTestMap(true);
    
    // Create a BSP tree for collision detection
    BSPTree collisionBSP;
    
    // Create sectors for the test map (matching the CUDA test map structure)
    std::vector<Sector> testMapSectors;
    
    // Main room sector
    Sector mainRoom;
    mainRoom.floorHeight = 0.0f;
    mainRoom.ceilingHeight = 2.0f;
    mainRoom.floorTextureId = 0;
    mainRoom.ceilingTextureId = 1;
    mainRoom.lightLevel = 200;
    mainRoom.tag = "main_room";
    
    // Main room walls (5x5 square, centered at origin)
    mainRoom.walls.push_back(Wall(Line(Vertex(-2.5f, 2.5f), Vertex(-0.5f, 2.5f)), 0, -1, 3));
    
    // Portal to corridor
    Wall portalWall = Wall(Line(Vertex(-0.5f, 2.5f), Vertex(0.5f, 2.5f)), 0, 1, 4);
    portalWall.isTransparent = true;
    mainRoom.walls.push_back(portalWall);
    
    // Rest of main room walls
    mainRoom.walls.push_back(Wall(Line(Vertex(0.5f, 2.5f), Vertex(2.5f, 2.5f)), 0, -1, 3));
    mainRoom.walls.push_back(Wall(Line(Vertex(2.5f, 2.5f), Vertex(2.5f, -2.5f)), 0, -1, 3));
    mainRoom.walls.push_back(Wall(Line(Vertex(2.5f, -2.5f), Vertex(-2.5f, -2.5f)), 0, -1, 3));
    mainRoom.walls.push_back(Wall(Line(Vertex(-2.5f, -2.5f), Vertex(-2.5f, 2.5f)), 0, -1, 3));
    
    // Corridor sector
    Sector corridor;
    corridor.floorHeight = 0.0f;
    corridor.ceilingHeight = 1.5f;
    corridor.floorTextureId = 0;
    corridor.ceilingTextureId = 1;
    corridor.lightLevel = 150;
    corridor.tag = "corridor";
    
    // Corridor walls
    // Connection to main room
    Wall corridorEntrance = Wall(Line(Vertex(0.5f, 2.5f), Vertex(-0.5f, 2.5f)), 1, 0, 4);
    corridorEntrance.isTransparent = true;
    corridor.walls.push_back(corridorEntrance);
    
    // Rest of corridor walls
    corridor.walls.push_back(Wall(Line(Vertex(-0.5f, 2.5f), Vertex(-0.5f, 4.5f)), 1, -1, 5));
    corridor.walls.push_back(Wall(Line(Vertex(-0.5f, 4.5f), Vertex(-0.25f, 4.5f)), 1, -1, 5));
    
    // Portal to side room
    Wall sideRoomPortal = Wall(Line(Vertex(-0.25f, 4.5f), Vertex(0.25f, 4.5f)), 1, 2, 4);
    sideRoomPortal.isTransparent = true;
    corridor.walls.push_back(sideRoomPortal);
    
    // Last corridor wall
    corridor.walls.push_back(Wall(Line(Vertex(0.25f, 4.5f), Vertex(0.5f, 4.5f)), 1, -1, 5));
    corridor.walls.push_back(Wall(Line(Vertex(0.5f, 4.5f), Vertex(0.5f, 2.5f)), 1, -1, 5));
    
    // Side room sector
    Sector sideRoom;
    sideRoom.floorHeight = 0.1f;
    sideRoom.ceilingHeight = 1.8f;
    sideRoom.floorTextureId = 2;
    sideRoom.ceilingTextureId = 1;
    sideRoom.lightLevel = 100;
    sideRoom.tag = "side_room";
    
    // Side room walls
    // Connection to corridor
    Wall sideRoomEntrance = Wall(Line(Vertex(0.25f, 4.5f), Vertex(-0.25f, 4.5f)), 2, 1, 4);
    sideRoomEntrance.isTransparent = true;
    sideRoom.walls.push_back(sideRoomEntrance);
    
    // Rest of side room walls
    sideRoom.walls.push_back(Wall(Line(Vertex(-0.25f, 4.5f), Vertex(-1.5f, 5.5f)), 2, -1, 6));
    sideRoom.walls.push_back(Wall(Line(Vertex(-1.5f, 5.5f), Vertex(1.5f, 5.5f)), 2, -1, 6));
    sideRoom.walls.push_back(Wall(Line(Vertex(1.5f, 5.5f), Vertex(0.25f, 4.5f)), 2, -1, 6));
    
    // Add sectors to the collection
    testMapSectors.push_back(mainRoom);
    testMapSectors.push_back(corridor);
    testMapSectors.push_back(sideRoom);
    
    // Build the BSP tree for collision detection
    collisionBSP.build(testMapSectors);
    
    // Main loop variables
    bool running = true;
    bool keyW = false, keyA = false, keyS = false, keyD = false;
    bool keyQ = false, keyE = false;
    SDL_Event event;
    
    // Movement speed
    const float moveSpeed = 0.1f;
    const float turnSpeed = 0.05f;
    
    // Player radius for collision detection
    const float PLAYER_RADIUS = 0.3f;
    
    // Add this near the top of the main() function before the main loop
    std::cout << "\n==== DEBUGGING INFORMATION ====\n";
    std::cout << "Test map enabled, player at: (0, 0)\n";
    std::cout << "Map contains:\n";
    std::cout << "  - Main room (5x5) with portal to corridor\n";
    std::cout << "  - Corridor from main room to side room\n";
    std::cout << "  - Side room with slightly elevated floor\n";
    std::cout << "Textures:\n";
    std::cout << "  - Checker floor (ID 0)\n";
    std::cout << "  - Ceiling texture (ID 1)\n";
    std::cout << "  - Side room floor (ID 2)\n";
    std::cout << "  - Wall textures (IDs 3-6)\n";
    std::cout << "Collision detection enabled with player radius: " << PLAYER_RADIUS << "\n";
    std::cout << "===============================\n";
    
    // Main loop
    while (running) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
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
                }
            }
        }
        
        // Calculate movement vector based on keyboard input
        Vec2 movementVector(0.0f, 0.0f);
        
        if (keyW) {
            movementVector.x += moveSpeed * cos(view.angle);
            movementVector.y += moveSpeed * sin(view.angle);
        }
        if (keyS) {
            movementVector.x -= moveSpeed * cos(view.angle);
            movementVector.y -= moveSpeed * sin(view.angle);
        }
        if (keyA) {
            movementVector.x += moveSpeed * cos(view.angle - M_PI / 2);
            movementVector.y += moveSpeed * sin(view.angle - M_PI / 2);
        }
        if (keyD) {
            movementVector.x += moveSpeed * cos(view.angle + M_PI / 2);
            movementVector.y += moveSpeed * sin(view.angle + M_PI / 2);
        }
        
        // Apply collision detection and response if we're trying to move
        if (movementVector.lengthSquared() > 0.001f) {
            // Store original position for unstick detection
            Vec2 originalPosition = view.position;
            
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
            
            if (collision.collision) {
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
                                
                                // Debug output for unsticking
                                std::cout << "Applying unstick vector: (" << unstickVector.x << ", " 
                                          << unstickVector.y << ")" << std::endl;
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
        
        // Render frame using test map
        cudaRenderer.renderTestMapFrame(view, 0.016f); // ~60fps
        
        // Get the rendered frame back
        cudaRenderer.retrieveRenderingResults(frameBuffer, zBuffer);
        
        // Update the SDL texture with our frame buffer
        SDL_UpdateTexture(frameTexture, NULL, frameBuffer.data(), WIDTH * sizeof(Color));
        
        // Clear screen
        SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
        SDL_RenderClear(sdlRenderer);
        
        // Draw the texture
        SDL_RenderCopy(sdlRenderer, frameTexture, NULL, NULL);
        
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