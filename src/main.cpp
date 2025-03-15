#include "BSPTree.h"
#include "Renderer.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <SDL.h>

using namespace PureDoom;

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
    
    // Screen dimensions
    const int SCREEN_WIDTH = 640;
    const int SCREEN_HEIGHT = 480;
    
    // Create SDL window
    SDL_Window* window = SDL_CreateWindow("PureDoom Renderer", 
                                          SDL_WINDOWPOS_UNDEFINED, 
                                          SDL_WINDOWPOS_UNDEFINED, 
                                          SCREEN_WIDTH, 
                                          SCREEN_HEIGHT, 
                                          SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return;
    }
    
    // Create SDL renderer
    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!sdlRenderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }
    
    // Create SDL texture to display the frame buffer
    SDL_Texture* texture = SDL_CreateTexture(sdlRenderer, 
                                            SDL_PIXELFORMAT_RGBA8888, 
                                            SDL_TEXTUREACCESS_STREAMING, 
                                            SCREEN_WIDTH, 
                                            SCREEN_HEIGHT);
    if (!texture) {
        std::cerr << "Texture creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(sdlRenderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }
    
    // Create our DOOM-style renderer
    Renderer renderer(SCREEN_WIDTH, SCREEN_HEIGHT);
    renderer.initialize();
    
    // Initial player position and movement variables
    ViewPosition view;
    view.position = Vec2(5.0f, 5.0f); // Start in the center of the first room
    view.angle = 0.0f;  // Looking east
    view.fov = 90.0f;   // 90 degree field of view
    view.height = 0.8f; // Player's eye height
    
    float moveSpeed = 0.05f;
    float rotateSpeed = 0.02f;
    
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
    
    std::cout << "\n--- Starting Rendering Loop ---\n";
    std::cout << "Use WASD to move, QE to rotate.\n";
    std::cout << "Press SPACE to trigger the door, ESC to quit.\n";
    
    while (!quit) {
        frameStart = SDL_GetTicks();
        
        // Calculate delta time for smooth movement
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
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
                        // Trigger the door
                        bsp.triggerSector("trigger_door");
                        std::cout << "Door triggered!" << std::endl;
                        break;
                }
            }
        }
        
        // Handle keyboard state for movement
        const Uint8* keystates = SDL_GetKeyboardState(NULL);
        
        // Calculate forward and right vectors based on view angle
        Vec2 forward(std::cos(view.angle), std::sin(view.angle));
        Vec2 right(std::cos(view.angle + PI/2), std::sin(view.angle + PI/2));
        
        // Move forward/backward
        if (keystates[SDL_SCANCODE_W]) {
            view.position = view.position + forward * moveSpeed;
        }
        if (keystates[SDL_SCANCODE_S]) {
            view.position = view.position - forward * moveSpeed;
        }
        
        // Strafe left/right
        if (keystates[SDL_SCANCODE_D]) {
            view.position = view.position + right * moveSpeed;
        }
        if (keystates[SDL_SCANCODE_A]) {
            view.position = view.position - right * moveSpeed;
        }
        
        // Rotate view
        if (keystates[SDL_SCANCODE_Q]) {
            view.angle -= rotateSpeed;
        }
        if (keystates[SDL_SCANCODE_E]) {
            view.angle += rotateSpeed;
        }
        
        // Update moving sectors
        bsp.update(deltaTime);
        
        // Render the frame
        renderer.renderFrame(bsp, view);
        
        // Update the SDL texture with our frame buffer
        SDL_UpdateTexture(texture, NULL, renderer.getFrameBuffer(), SCREEN_WIDTH * 4);
        
        // Clear the SDL renderer and render the texture
        SDL_RenderClear(sdlRenderer);
        SDL_RenderCopy(sdlRenderer, texture, NULL, NULL);
        SDL_RenderPresent(sdlRenderer);
        
        // Cap the frame rate
        frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_TIME) {
            SDL_Delay(FRAME_TIME - frameTime);
        }
    }
    
    // Clean up SDL resources
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main() {
    std::cout << "PureDoom - Enhanced BSP Tree Implementation with Renderer\n";
    std::cout << "=======================================================\n\n";
    
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