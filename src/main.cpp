#include "BSPTree.h"
#include "Renderer.h"
#include "Sprite.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <SDL.h>

using namespace PureDoom;

// Function to create test sprites for the map
std::vector<Sprite> createTestSprites() {
    std::vector<Sprite> sprites;
    
    // Create a blue circular sprite in the main room
    Sprite blueSprite(Vec2(5.0f, 5.0f), 0.7f, SpriteType::PROP);
    blueSprite.addFrame(10, 1.0f, 1.0f); // Use a lower texture ID that actually exists
    blueSprite.scale = 0.8f;
    blueSprite.lightLevel = 200;
    blueSprite.tag = "blue_orb";
    sprites.push_back(blueSprite);
    
    // Create a green item in the second room
    Sprite greenItem(Vec2(15.0f, 5.0f), 0.5f, SpriteType::ITEM);
    greenItem.addFrame(11, 0.8f, 0.8f); // Use a lower texture ID that actually exists
    greenItem.scale = 0.6f;
    greenItem.lightLevel = 255;
    greenItem.tag = "green_gem";
    sprites.push_back(greenItem);
    
    // Create a red enemy near the elevator
    Sprite redEnemy(Vec2(17.0f, 11.0f), 0.8f, SpriteType::ENEMY);
    redEnemy.addFrame(12, 1.0f, 1.5f); // Use a lower texture ID that actually exists
    redEnemy.scale = 1.0f;
    redEnemy.lightLevel = 180;
    redEnemy.tag = "red_enemy";
    sprites.push_back(redEnemy);
    
    // Add some more items in various locations
    for (int i = 0; i < 4; i++) {
        float x = 2.0f + i * 2.0f;
        float y = 2.0f + i * 1.5f;
        
        Sprite item(Vec2(x, y), 0.5f, SpriteType::ITEM);
        item.addFrame(11, 0.8f, 0.8f); // Use a lower texture ID that actually exists
        item.scale = 0.5f;
        item.lightLevel = 220;
        item.tag = "item_" + std::to_string(i);
        sprites.push_back(item);
    }
    
    // Add some enemies in the door room
    Sprite doorEnemy(Vec2(6.0f, 12.0f), 0.8f, SpriteType::ENEMY);
    doorEnemy.addFrame(12, 1.0f, 1.5f); // Use a lower texture ID that actually exists
    doorEnemy.scale = 0.9f;
    doorEnemy.lightLevel = 150;
    doorEnemy.tag = "door_guard";
    sprites.push_back(doorEnemy);
    
    std::cout << "Created " << sprites.size() << " test sprites\n";
    return sprites;
}

// Function to create an enhanced test map with more features
std::vector<Sector> createEnhancedTestMap() {
    std::vector<Sector> sectors;
    
    // Create a main room (sector 0)
    Sector mainRoom;
    mainRoom.floorHeight = 0.0f;
    mainRoom.ceilingHeight = 3.0f;
    mainRoom.floorTextureId = 1;
    mainRoom.ceilingTextureId = 2;
    mainRoom.lightLevel = 128;
    mainRoom.tag = "main_room";
    
    // Walls for main room (square room)
    mainRoom.walls.push_back(Wall(Line(Vertex(0.0f, 0.0f), Vertex(10.0f, 0.0f)), 0, -1, 3));
    mainRoom.walls.push_back(Wall(Line(Vertex(10.0f, 0.0f), Vertex(10.0f, 10.0f)), 0, -1, 4));
    mainRoom.walls.push_back(Wall(Line(Vertex(10.0f, 10.0f), Vertex(0.0f, 10.0f)), 0, -1, 5));
    mainRoom.walls.push_back(Wall(Line(Vertex(0.0f, 10.0f), Vertex(0.0f, 0.0f)), 0, -1, 6));
    
    // Create a second room (sector 1)
    Sector secondRoom;
    secondRoom.floorHeight = 0.0f;
    secondRoom.ceilingHeight = 3.0f;
    secondRoom.floorTextureId = 7;
    secondRoom.ceilingTextureId = 8;
    secondRoom.lightLevel = 200;
    secondRoom.tag = "second_room";
    
    // Walls for second room
    secondRoom.walls.push_back(Wall(Line(Vertex(10.0f, 0.0f), Vertex(20.0f, 0.0f)), 1, -1, 9));
    secondRoom.walls.push_back(Wall(Line(Vertex(20.0f, 0.0f), Vertex(20.0f, 10.0f)), 1, -1, 10));
    secondRoom.walls.push_back(Wall(Line(Vertex(20.0f, 10.0f), Vertex(10.0f, 10.0f)), 1, -1, 11));
    
    // Portal wall connecting rooms (note: two-way portal)
    Wall portalWall = Wall(Line(Vertex(10.0f, 10.0f), Vertex(10.0f, 0.0f)), 1, 0, 12);
    portalWall.isTransparent = true;  // Can see through this wall
    portalWall.tag = "main_portal";
    secondRoom.walls.push_back(portalWall);
    
    // Update the corresponding wall in the first room to be a portal as well
    mainRoom.walls[1].sectorBack = 1;
    mainRoom.walls[1].isTransparent = true;
    mainRoom.walls[1].tag = "main_portal";
    
    // Create a third room - this will be a moving elevator (sector 2)
    Sector elevatorRoom;
    elevatorRoom.floorHeight = 0.0f;
    elevatorRoom.ceilingHeight = 3.0f;
    elevatorRoom.floorTextureId = 13;
    elevatorRoom.ceilingTextureId = 14;
    elevatorRoom.lightLevel = 150;
    elevatorRoom.tag = "elevator";
    elevatorRoom.type = SectorType::ELEVATOR;
    
    // Configure elevator movement
    elevatorRoom.floorMovement = MovementType::SINE_WAVE;
    elevatorRoom.movementSpeed = 0.5f;        // Speed of movement
    elevatorRoom.movementDistance = 2.0f;     // Move up/down by 2 units
    elevatorRoom.movementActive = true;       // Start active
    
    // Walls for elevator room (small room connected to second room)
    elevatorRoom.walls.push_back(Wall(Line(Vertex(15.0f, 10.0f), Vertex(20.0f, 10.0f)), 2, 1, 15));
    elevatorRoom.walls.push_back(Wall(Line(Vertex(20.0f, 10.0f), Vertex(20.0f, 12.0f)), 2, -1, 16));
    elevatorRoom.walls.push_back(Wall(Line(Vertex(20.0f, 12.0f), Vertex(15.0f, 12.0f)), 2, -1, 17));
    elevatorRoom.walls.push_back(Wall(Line(Vertex(15.0f, 12.0f), Vertex(15.0f, 10.0f)), 2, -1, 18));
    
    // Update the corresponding wall in the second room to be a portal to the elevator
    secondRoom.walls[2].sectorBack = 2;
    secondRoom.walls[2].isTransparent = true;
    secondRoom.walls[2].tag = "elevator_entrance";
    
    // Create a fourth room - a door that can be triggered (sector 3)
    Sector doorRoom;
    doorRoom.floorHeight = 0.0f;
    doorRoom.ceilingHeight = 3.0f;
    doorRoom.floorTextureId = 19;
    doorRoom.ceilingTextureId = 20;
    doorRoom.lightLevel = 100;
    doorRoom.tag = "door_room";
    doorRoom.type = SectorType::DOOR;
    
    // Configure door movement
    doorRoom.ceilingMovement = MovementType::TRIGGERED_ONCE;
    doorRoom.movementSpeed = 1.0f;          // Speed of door
    doorRoom.movementDistance = -3.0f;      // Move ceiling down by 3 units (open door)
    doorRoom.movementActive = false;        // Start closed
    doorRoom.triggerTag = "trigger_door";   // Tag to trigger this door
    
    // Walls for door room (connected to main room)
    doorRoom.walls.push_back(Wall(Line(Vertex(5.0f, 10.0f), Vertex(7.0f, 10.0f)), 3, 0, 21));
    doorRoom.walls.push_back(Wall(Line(Vertex(7.0f, 10.0f), Vertex(7.0f, 15.0f)), 3, -1, 22));
    doorRoom.walls.push_back(Wall(Line(Vertex(7.0f, 15.0f), Vertex(5.0f, 15.0f)), 3, -1, 23));
    doorRoom.walls.push_back(Wall(Line(Vertex(5.0f, 15.0f), Vertex(5.0f, 10.0f)), 3, -1, 24));
    
    // Update the corresponding wall in the main room to be a portal to the door
    // This is a bit hacky since we don't have proper concave sector support
    // In a real implementation, you'd have proper wall indexing
    mainRoom.walls[2].sectorBack = 3;
    mainRoom.walls[2].isTransparent = false; // Door is initially closed
    mainRoom.walls[2].tag = "door_entrance";
    
    // Add all rooms to the sectors list
    sectors.push_back(mainRoom);
    sectors.push_back(secondRoom);
    sectors.push_back(elevatorRoom);
    sectors.push_back(doorRoom);
    
    std::cout << "Created enhanced test map with " << sectors.size() << " sectors\n";
    return sectors;
}

// Test enhanced collision detection
void testEnhancedCollision(const BSPTree& bsp) {
    std::cout << "\n--- Enhanced Collision Test ---\n";
    
    // Test various collision scenarios
    struct CollisionTest {
        Vec2 position;
        Vec2 direction;
        float radius;
        float distance;
        std::string description;
    };
    
    std::vector<CollisionTest> tests = {
        {Vec2(5.0f, 5.0f), Vec2(1.0f, 0.0f), 0.5f, 10.0f, "From center of room 1 toward room 2"},
        {Vec2(5.0f, 5.0f), Vec2(0.0f, 1.0f), 0.5f, 10.0f, "From center of room 1 toward north wall"},
        {Vec2(15.0f, 5.0f), Vec2(-1.0f, 0.0f), 0.5f, 10.0f, "From center of room 2 toward room 1"},
        {Vec2(15.0f, 11.0f), Vec2(0.0f, 1.0f), 0.5f, 10.0f, "From near elevator entrance toward elevator"}
    };
    
    for (const auto& test : tests) {
        // Test ray casting with enhanced collision info
        CollisionInfo rayCollision = bsp.castRay(test.position, test.direction, test.distance);
        
        std::cout << "Ray collision test: " << test.description << "\n";
        std::cout << "  Position: (" << test.position.x << ", " << test.position.y << ")\n";
        std::cout << "  Direction: (" << test.direction.x << ", " << test.direction.y << ")\n";
        
        if (rayCollision.collision) {
            std::cout << "  Hit at point: (" << rayCollision.point.x << ", " << rayCollision.point.y << ")\n";
            std::cout << "  Distance: " << rayCollision.distance << "\n";
            std::cout << "  Normal: (" << rayCollision.normal.x << ", " << rayCollision.normal.y << ")\n";
            std::cout << "  Sector: " << rayCollision.sectorId << "\n";
        } else {
            std::cout << "  No collision detected.\n";
        }
        
        // Test physics collision with radius
        Vec2 velocity = test.direction * test.distance;
        CollisionInfo physicsCollision = bsp.checkCollision(test.position, test.radius, velocity);
        
        std::cout << "Physics collision test:\n";
        if (physicsCollision.collision) {
            std::cout << "  Collision at " << (physicsCollision.distance * 100.0f) << "% of movement\n";
            std::cout << "  Point: (" << physicsCollision.point.x << ", " << physicsCollision.point.y << ")\n";
            std::cout << "  Normal: (" << physicsCollision.normal.x << ", " << physicsCollision.normal.y << ")\n";
            
            // Calculate slide vector (how an object would slide along a wall)
            Vec2 slideVec = velocity * (1.0f - physicsCollision.distance);
            Vec2 normalComponent = physicsCollision.normal * slideVec.dotProduct(physicsCollision.normal);
            Vec2 tangentComponent = slideVec - normalComponent;
            
            std::cout << "  Slide vector: (" << tangentComponent.x << ", " << tangentComponent.y << ")\n";
        } else {
            std::cout << "  No physics collision detected.\n";
        }
        
        std::cout << "\n";
    }
}

// Test sector visibility
void testSectorVisibility(const BSPTree& bsp) {
    std::cout << "\n--- Sector Visibility Test ---\n";
    
    // Test visibility from different viewpoints
    struct VisibilityTest {
        Vec2 position;
        float angle;
        float fov;
        std::string description;
    };
    
    std::vector<VisibilityTest> tests = {
        {Vec2(5.0f, 5.0f), 0.0f, 90.0f, "From center of room 1 looking east"},
        {Vec2(5.0f, 5.0f), 90.0f, 90.0f, "From center of room 1 looking north"},
        {Vec2(15.0f, 5.0f), 180.0f, 90.0f, "From center of room 2 looking west"},
        {Vec2(15.0f, 11.0f), 90.0f, 90.0f, "From near elevator entrance looking north"}
    };
    
    for (const auto& test : tests) {
        int viewerSector = bsp.findSector(test.position);
        
        std::cout << "Visibility test: " << test.description << "\n";
        std::cout << "  Position: (" << test.position.x << ", " << test.position.y << ") in sector " << viewerSector << "\n";
        std::cout << "  Angle: " << test.angle << " degrees, FOV: " << test.fov << " degrees\n";
        
        // Check visibility of all sectors
        for (size_t i = 0; i < bsp.getSectors().size(); i++) {
            bool visible = bsp.isSectorVisible(static_cast<int>(i), test.position, test.angle, test.fov);
            std::cout << "  Sector " << i << " is " << (visible ? "visible" : "not visible") << "\n";
        }
        
        // Get all visible portals
        std::vector<int> visibleSectors = bsp.findVisiblePortals(viewerSector, test.position, test.angle, test.fov);
        
        std::cout << "  Visible sectors through portals: ";
        if (visibleSectors.empty()) {
            std::cout << "none";
        } else {
            for (size_t i = 0; i < visibleSectors.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << visibleSectors[i];
            }
        }
        std::cout << "\n\n";
    }
}

// Function to test moving sectors
void testMovingSectors(BSPTree& bsp) {
    std::cout << "\n--- Moving Sectors Test ---\n";
    
    // Simulate a few seconds of movement
    const float timeStep = 0.1f;
    const float totalTime = 3.0f;
    
    std::cout << "Simulating sector movement for " << totalTime << " seconds...\n";
    
    for (float time = 0; time <= totalTime; time += timeStep) {
        bsp.update(timeStep);
        
        // In a real game, you'd render or update physics here
        // For this test, we'll just sleep a bit to simulate the passage of time
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        std::cout << "Time: " << time << "s\n";
        
        // Every second, trigger the door
        if (fmod(time, 1.0f) < timeStep) {
            std::cout << "Triggering door at time " << time << "s\n";
            bsp.triggerSector("trigger_door");
        }
    }
    
    std::cout << "Movement simulation complete.\n";
}

// New function for rendering with SDL2
void renderWithSDL(BSPTree& bsp) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return;
    }
    
    // Window dimensions (display size)
    const int WINDOW_WIDTH = 1280;
    const int WINDOW_HEIGHT = 960;
    
    // Render dimensions (actual rendering resolution, can be adjusted at runtime)
    // Using variable instead of const for resolution scaling to allow runtime changes
    float scalingFactor = 0.5f; // Start at half resolution for much better performance
    int renderWidth = static_cast<int>(WINDOW_WIDTH * scalingFactor);
    int renderHeight = static_cast<int>(WINDOW_HEIGHT * scalingFactor);
    
    // Create SDL window
    SDL_Window* window = SDL_CreateWindow("PureDoom Renderer with Sprites", 
                                          SDL_WINDOWPOS_UNDEFINED, 
                                          SDL_WINDOWPOS_UNDEFINED, 
                                          WINDOW_WIDTH, 
                                          WINDOW_HEIGHT, 
                                          SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return;
    }
    
    // Create SDL renderer with hardware acceleration
    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!sdlRenderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }
    
    // Enable linear scaling for smoother appearance when upscaling
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    
    // Create render texture
    SDL_Texture* renderTexture = SDL_CreateTexture(sdlRenderer, 
                                                SDL_PIXELFORMAT_RGBA8888, 
                                                SDL_TEXTUREACCESS_STREAMING, 
                                                renderWidth, 
                                                renderHeight);
    
    if (!renderTexture) {
        std::cerr << "Render texture creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(sdlRenderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }
    
    // Create our DOOM-style renderer with the render resolution
    Renderer renderer(renderWidth, renderHeight);
    renderer.initialize();
    
    // Add GPU acceleration toggle
    bool gpuAccelerationEnabled = renderer.isGpuAccelerationEnabled();
    
    // Performance variables
    bool showFPS = true;
    int frameCount = 0;
    float fpsTimer = 0.0f;
    float currentFPS = 0.0f;
    
    // Key state tracking for toggling options
    bool keyPressedP = false; // Performance mode
    bool keyPressedO = false; // FPS display
    bool keyPressedLeftBracket = false; // Decrease resolution
    bool keyPressedRightBracket = false; // Increase resolution
    bool keyPressedF = false; // Dynamic sky
    bool keyPressedB = false; // Speed up time
    bool keyPressedN = false; // Slow down time
    bool keyPressedK = false; // Increase sun size
    bool keyPressedL = false; // Decrease sun size
    bool keyPressedG = false; // Toggle GPU acceleration
    
    // Create test sprites
    std::vector<Sprite> sprites = createTestSprites();
    
    // Get skybox reference
    Skybox& skybox = renderer.getSkybox();
    
    // Create a performance mode flag that can be toggled
    bool performanceMode = true;
    float maxViewDistance = performanceMode ? 15.0f : 30.0f; // Reduced view distance in performance mode
    
    std::cout << "\n--- Created " << sprites.size() << " sprites ---\n";
    for (const auto& sprite : sprites) {
        std::cout << "Sprite: " << sprite.tag 
                  << ", Position: (" << sprite.position.x << ", " << sprite.position.y << ")"
                  << ", Type: " << static_cast<int>(sprite.type)
                  << ", Texture ID: " << sprite.getCurrentFrame().textureId
                  << ", Scale: " << sprite.scale
                  << ", Height offset: " << sprite.heightOffset
                  << ", Light level: " << sprite.lightLevel << std::endl;
    }
    std::cout << "-----------------------------\n\n";
    
    std::cout << "\n--- Performance Settings ---\n";
    std::cout << "Window size: " << WINDOW_WIDTH << "x" << WINDOW_HEIGHT << "\n";
    std::cout << "Render resolution: " << renderWidth << "x" << renderHeight << " (scaling factor: " << scalingFactor << ")\n";
    std::cout << "Performance mode: " << (performanceMode ? "ON" : "OFF") << "\n";
    std::cout << "GPU acceleration: " << (gpuAccelerationEnabled ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "Max view distance: " << maxViewDistance << " units\n";
    std::cout << "Press P to toggle performance mode\n";
    std::cout << "Press O to toggle FPS display\n";
    std::cout << "Press [ to decrease rendering resolution\n";
    std::cout << "Press ] to increase rendering resolution\n";
    std::cout << "Press G to toggle GPU acceleration\n\n";
    
    std::cout << "\n--- Starting Rendering Loop ---\n";
    std::cout << "Use WASD to move, QE to rotate, or move the mouse to look around.\n";
    std::cout << "Press SPACE to jump, C to crouch, M to toggle mouse control, and ESCAPE to quit.\n";
    std::cout << "Skybox Controls:\n";
    std::cout << "  F - Toggle dynamic sky on/off\n";
    std::cout << "  B - Speed up time of day\n";
    std::cout << "  N - Slow down time of day\n";
    std::cout << "  K - Increase sun size\n";
    std::cout << "  L - Decrease sun size\n";
    
    // Initial player position and movement variables
    ViewPosition view;
    view.position = Vec2(5.0f, 5.0f); // Start in the center of the first room
    view.angle = 0.0f;  // Looking east
    view.fov = 90.0f;   // 90 degree field of view
    view.height = 0.8f; // Player's eye height
    
    float moveSpeed = 0.05f;
    float rotateSpeed = 0.05f;
    
    // Jump physics variables
    const float PLAYER_DEFAULT_HEIGHT = 0.8f;
    const float PLAYER_CROUCH_HEIGHT = 0.4f;
    const float JUMP_INITIAL_VELOCITY = 0.08f;
    const float GRAVITY = 0.004f;
    float verticalVelocity = 0.0f;
    bool isJumping = false;
    bool isCrouching = false;
    
    // Mouse control variables
    bool mouseControlEnabled = true;
    const float mouseSensitivity = 0.003f;
    int mouseX = renderWidth / 2;
    int mouseY = renderHeight / 2;
    
    // Hide cursor and enable relative mouse mode when mouse control is active
    if (mouseControlEnabled) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }
    
    // Main loop
    bool quit = false;
    SDL_Event e;
    
    // For controlling frame rate
    const int FPS = 60;
    const int FRAME_TIME = 1000 / FPS;
    Uint32 frameStart;
    int frameTime;
    
    // For calculating deltaTime
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    // Variables for sprite animation
    float spriteAnimTimer = 0.0f;
    const float spriteAnimRate = 1.0f;  // Animation speed in seconds
    
    // Flag to indicate if renderer needs to be recreated
    bool recreateRenderer = false;
    
    while (!quit) {
        frameStart = SDL_GetTicks();
        
        // Calculate delta time for smooth movement
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        // Update FPS counter
        frameCount++;
        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f) {
            currentFPS = frameCount / fpsTimer;
            frameCount = 0;
            fpsTimer = 0.0f;
            if (showFPS) {
                std::cout << "FPS: " << currentFPS << std::endl;
            }
        }
        
        // Handle events
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        quit = true;
                        break;
                    case SDLK_SPACE:
                        // Jump if on the ground
                        if (!isJumping && !isCrouching) {
                            isJumping = true;
                            verticalVelocity = JUMP_INITIAL_VELOCITY;
                            std::cout << "Player jumped!" << std::endl;
                        }
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
                        break;
                    case SDLK_m:
                        // Toggle mouse control
                        mouseControlEnabled = !mouseControlEnabled;
                        SDL_SetRelativeMouseMode(mouseControlEnabled ? SDL_TRUE : SDL_FALSE);
                        std::cout << "Mouse control " << (mouseControlEnabled ? "enabled" : "disabled") << std::endl;
                        break;
                    case SDLK_t:
                        // Trigger the door (changed from SPACE to T to avoid conflict with jump)
                        bsp.triggerSector("trigger_door");
                        std::cout << "Door triggered!" << std::endl;
                        break;
                    case SDLK_n:
                        // Toggle minimap
                        renderer.setMinimapEnabled(!renderer.isMinimapEnabled());
                        std::cout << "Minimap " << (renderer.isMinimapEnabled() ? "enabled" : "disabled") << std::endl;
                        break;
                }
            } else if (e.type == SDL_MOUSEMOTION && mouseControlEnabled) {
                // Apply mouse movement to camera rotation
                view.angle += e.motion.xrel * mouseSensitivity;
                
                // Optional: Implement vertical look (requires additional view pitch variable)
                // verticalLook += e.motion.yrel * mouseSensitivity;
                // verticalLook = std::max(-1.0f, std::min(1.0f, verticalLook)); // Clamp between -1 and 1
            } else if (e.type == SDL_MOUSEBUTTONDOWN && mouseControlEnabled) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    // Left mouse button - could be used for shooting or interaction
                    std::cout << "Left mouse button pressed" << std::endl;
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    // Right mouse button - could be used for alt fire or secondary action
                    std::cout << "Right mouse button pressed" << std::endl;
                    
                    // Example: Trigger the door with right click
                    bsp.triggerSector("trigger_door");
                }
            }
        }
        
        // Direct key state handling for performance controls
        const Uint8* keystates = SDL_GetKeyboardState(NULL);
        
        // Toggle performance mode (P key)
        if (keystates[SDL_SCANCODE_P] && !keyPressedP) {
            keyPressedP = true;
            performanceMode = !performanceMode;
            maxViewDistance = performanceMode ? 15.0f : 30.0f;
            skybox.dynamicSky = !performanceMode; // Disable dynamic sky in performance mode
            std::cout << "Performance mode: " << (performanceMode ? "ON" : "OFF") << std::endl;
            std::cout << "Max view distance: " << maxViewDistance << " units" << std::endl;
        } else if (!keystates[SDL_SCANCODE_P]) {
            keyPressedP = false;
        }
        
        // Toggle FPS display (O key)
        if (keystates[SDL_SCANCODE_O] && !keyPressedO) {
            keyPressedO = true;
            showFPS = !showFPS;
            std::cout << "FPS display: " << (showFPS ? "ON" : "OFF") << std::endl;
        } else if (!keystates[SDL_SCANCODE_O]) {
            keyPressedO = false;
        }
        
        // Decrease rendering resolution ([ key)
        if (keystates[SDL_SCANCODE_LEFTBRACKET] && !keyPressedLeftBracket) {
            keyPressedLeftBracket = true;
            if (scalingFactor > 0.3f) {
                scalingFactor -= 0.1f;
                renderWidth = static_cast<int>(WINDOW_WIDTH * scalingFactor);
                renderHeight = static_cast<int>(WINDOW_HEIGHT * scalingFactor);
                recreateRenderer = true;
                std::cout << "Decreasing rendering resolution to " << renderWidth << "x" << renderHeight 
                          << " (scale: " << scalingFactor << ")" << std::endl;
            }
        } else if (!keystates[SDL_SCANCODE_LEFTBRACKET]) {
            keyPressedLeftBracket = false;
        }
        
        // Increase rendering resolution (] key)
        if (keystates[SDL_SCANCODE_RIGHTBRACKET] && !keyPressedRightBracket) {
            keyPressedRightBracket = true;
            if (scalingFactor < 1.0f) {
                scalingFactor += 0.1f;
                renderWidth = static_cast<int>(WINDOW_WIDTH * scalingFactor);
                renderHeight = static_cast<int>(WINDOW_HEIGHT * scalingFactor);
                recreateRenderer = true;
                std::cout << "Increasing rendering resolution to " << renderWidth << "x" << renderHeight 
                          << " (scale: " << scalingFactor << ")" << std::endl;
            }
        } else if (!keystates[SDL_SCANCODE_RIGHTBRACKET]) {
            keyPressedRightBracket = false;
        }
        
        // Toggle dynamic sky (F key)
        if (keystates[SDL_SCANCODE_F] && !keyPressedF) {
            keyPressedF = true;
            skybox.dynamicSky = !skybox.dynamicSky;
            std::cout << "Dynamic sky " << (skybox.dynamicSky ? "enabled" : "disabled") << std::endl;
        } else if (!keystates[SDL_SCANCODE_F]) {
            keyPressedF = false;
        }
        
        // Speed up time (B key)
        if (keystates[SDL_SCANCODE_B] && !keyPressedB) {
            keyPressedB = true;
            skybox.timeOfDay += 0.05f;
            if (skybox.timeOfDay >= 1.0f) {
                skybox.timeOfDay -= 1.0f;
            }
            std::cout << "Time of day: " << (skybox.timeOfDay * 24.0f) << " hours" << std::endl;
        } else if (!keystates[SDL_SCANCODE_B]) {
            keyPressedB = false;
        }
        
        // Slow down time (N key)
        if (keystates[SDL_SCANCODE_N] && !keyPressedN) {
            keyPressedN = true;
            skybox.timeOfDay -= 0.05f;
            if (skybox.timeOfDay < 0.0f) {
                skybox.timeOfDay += 1.0f;
            }
            std::cout << "Time of day: " << (skybox.timeOfDay * 24.0f) << " hours" << std::endl;
        } else if (!keystates[SDL_SCANCODE_N]) {
            keyPressedN = false;
        }
        
        // Increase sun size (K key)
        if (keystates[SDL_SCANCODE_K] && !keyPressedK) {
            keyPressedK = true;
            skybox.sunSize += 1.0f;
            skybox.sunSize = std::min(20.0f, skybox.sunSize);
            std::cout << "Sun size: " << skybox.sunSize << " degrees" << std::endl;
        } else if (!keystates[SDL_SCANCODE_K]) {
            keyPressedK = false;
        }
        
        // Decrease sun size (L key)
        if (keystates[SDL_SCANCODE_L] && !keyPressedL) {
            keyPressedL = true;
            skybox.sunSize -= 1.0f;
            skybox.sunSize = std::max(1.0f, skybox.sunSize);
            std::cout << "Sun size: " << skybox.sunSize << " degrees" << std::endl;
        } else if (!keystates[SDL_SCANCODE_L]) {
            keyPressedL = false;
        }
        
        // Toggle GPU acceleration (G key)
        if (keystates[SDL_SCANCODE_G] && !keyPressedG) {
            keyPressedG = true;
            gpuAccelerationEnabled = !gpuAccelerationEnabled;
            renderer.setGpuAccelerationEnabled(gpuAccelerationEnabled);
            std::cout << "GPU acceleration: " << (gpuAccelerationEnabled ? "ENABLED" : "DISABLED") << std::endl;
        } else if (!keystates[SDL_SCANCODE_G]) {
            keyPressedG = false;
        }
        
        // Recreate renderer if needed (resolution change)
        if (recreateRenderer) {
            // Destroy old texture
            SDL_DestroyTexture(renderTexture);
            
            // Create new texture with updated dimensions
            renderTexture = SDL_CreateTexture(sdlRenderer, 
                                             SDL_PIXELFORMAT_RGBA8888, 
                                             SDL_TEXTUREACCESS_STREAMING, 
                                             renderWidth, 
                                             renderHeight);
            
            // Create new renderer (properly handling unique_ptr ownership)
            // Using std::move to transfer ownership since Renderer contains a unique_ptr
            renderer = std::move(Renderer(renderWidth, renderHeight));
            renderer.initialize();
            
            // Restore GPU acceleration setting
            renderer.setGpuAccelerationEnabled(gpuAccelerationEnabled);
            
            // Reset flag
            recreateRenderer = false;
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
                view.height = targetHeight;
                verticalVelocity = 0.0f;
                isJumping = false;
                std::cout << "Player landed" << std::endl;
            }
        }
        
        // Handle keyboard state for movement
        // Calculate forward and right vectors based on view angle
        Vec2 forward(std::cos(view.angle), std::sin(view.angle));
        Vec2 right(std::cos(view.angle + PI/2), std::sin(view.angle + PI/2));
        
        // Movement speed affected by crouch state
        float currentMoveSpeed = moveSpeed;
        if (isCrouching) {
            currentMoveSpeed *= 0.5f; // Move slower when crouched
        }
        
        // Move forward/backward
        if (keystates[SDL_SCANCODE_W]) {
            view.position = view.position + forward * currentMoveSpeed;
        }
        if (keystates[SDL_SCANCODE_S]) {
            view.position = view.position - forward * currentMoveSpeed;
        }
        
        // Strafe left/right
        if (keystates[SDL_SCANCODE_D]) {
            view.position = view.position + right * currentMoveSpeed;
        }
        if (keystates[SDL_SCANCODE_A]) {
            view.position = view.position - right * currentMoveSpeed;
        }
        
        // Rotate view with keyboard (only if mouse control is disabled or if keys are pressed)
        if (!mouseControlEnabled || keystates[SDL_SCANCODE_Q]) {
            view.angle -= rotateSpeed;
        }
        if (!mouseControlEnabled || keystates[SDL_SCANCODE_E]) {
            view.angle += rotateSpeed;
        }
        
        // Update moving sectors
        bsp.update(deltaTime);
        
        // Update sprites
        spriteAnimTimer += deltaTime;
        if (spriteAnimTimer >= spriteAnimRate) {
            spriteAnimTimer -= spriteAnimRate;
            
            // Rotate some sprites by adjusting their positions
            for (auto& sprite : sprites) {
                if (sprite.type == SpriteType::ITEM) {
                    // Make items bob up and down
                    sprite.heightOffset = 0.5f + 0.1f * sin(spriteAnimTimer * 2.0f * PI);
                }
                else if (sprite.type == SpriteType::ENEMY) {
                    // Make enemies move slightly
                    float angle = spriteAnimTimer * 2.0f * PI;
                    Vec2 offset(0.2f * sin(angle), 0.2f * cos(angle));
                    sprite.position = sprite.position + offset * deltaTime;
                }
            }
            
            // Update sprite animations (if they had any)
            for (auto& sprite : sprites) {
                sprite.update(deltaTime);
            }
        }
        
        // Pass max view distance to renderer through skybox (since we already have a reference)
        skybox.maxViewDistance = maxViewDistance;
        
        // Render the frame with sprites
        renderer.renderFrame(bsp, view, sprites);
        
        // Update the SDL texture with our frame buffer
        SDL_UpdateTexture(renderTexture, NULL, renderer.getFrameBuffer(), renderWidth * 4);
        
        // Clear the SDL renderer and render the texture with scaling to fit the window
        SDL_RenderClear(sdlRenderer);
        SDL_RenderCopy(sdlRenderer, renderTexture, NULL, NULL);
        
        // Display the rendered frame
        SDL_RenderPresent(sdlRenderer);
        
        // Cap the frame rate
        frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_TIME) {
            SDL_Delay(FRAME_TIME - frameTime);
        }
    }
    
    // Clean up SDL resources
    SDL_SetRelativeMouseMode(SDL_FALSE);  // Ensure mouse is visible when exiting
    SDL_DestroyTexture(renderTexture);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main() {
    std::cout << "PureDoom - Enhanced BSP Tree Implementation with Renderer\n";
    std::cout << "=======================================================\n\n";
    std::cout << "DOOM-Style Rendering Features:\n";
    std::cout << "- Column-based wall rendering (raycasting)\n";
    std::cout << "- Span-based floor and ceiling rendering\n";
    std::cout << "- Texture mapping with perspective correction\n";
    std::cout << "- Visplane optimization for efficient rendering\n";
    std::cout << "- Billboarded sprite rendering for entities\n";
    std::cout << "- Distance-based fog and lighting effects\n";
    std::cout << "- CUDA GPU acceleration for performance-intensive operations\n\n";
    
    // Create enhanced test map
    std::vector<Sector> testMap = createEnhancedTestMap();
    
    std::cout << "Test map created with " << testMap.size() << " sectors:\n";
    for (size_t i = 0; i < testMap.size(); ++i) {
        const Sector& sector = testMap[i];
        std::cout << "Sector " << i << " (Tag: '" << sector.tag << "'): "
                 << sector.walls.size() << " walls, ";
        
        if (sector.type != SectorType::NORMAL) {
            std::cout << "Type: ";
            switch (sector.type) {
                case SectorType::DOOR: std::cout << "DOOR"; break;
                case SectorType::ELEVATOR: std::cout << "ELEVATOR"; break;
                case SectorType::CRUSHER: std::cout << "CRUSHER"; break;
                case SectorType::DAMAGING: std::cout << "DAMAGING"; break;
                case SectorType::SPECIAL: std::cout << "SPECIAL"; break;
                default: std::cout << "NORMAL"; break;
            }
            
            if (sector.isMoving()) {
                std::cout << ", Moving";
                if (sector.floorMovement != MovementType::NONE) {
                    std::cout << " Floor";
                }
                if (sector.ceilingMovement != MovementType::NONE) {
                    std::cout << " Ceiling";
                }
            }
        }
        std::cout << "\n";
    }
    
    std::cout << "Building BSP tree...\n";
    
    // Create and build the BSP tree
    BSPTree bsp;
    try {
        bsp.build(testMap);
        std::cout << "BSP tree build successful!\n";
        
        // Validate the BSP tree
        if (bsp.validate()) {
            std::cout << "BSP tree validation successful.\n";
            
            // Start the SDL renderer
            renderWithSDL(bsp);
        } else {
            std::cout << "Warning: BSP tree validation failed.\n";
        }
    }
    catch(const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
    catch(...) {
        std::cerr << "Unknown exception caught!" << std::endl;
    }
    
    return 0;
} 