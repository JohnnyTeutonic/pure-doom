#include "RendererCuda.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <iostream>
#include <cmath>      // For std::fmod
#include <algorithm>  // For std::max, std::min

// Host and device implementations of Color methods for CUDA kernels
namespace PureDoom {

// Host/device implementation of Color::fromHSV
__host__ __device__ Color Color::fromHSV(float h, float s, float v) {
    if (s <= 0.0f) {
        uint8_t value = static_cast<uint8_t>(v * 255.0f);
        Color result;
        result.r = value;
        result.g = value;
        result.b = value;
        result.a = 255;
        return result;
    }
    
    // Use appropriate math functions depending on compilation context
    #if defined(__CUDA_ARCH__)
    // Device code path (inside CUDA kernel)
    h = fmodf(h, 360.0f) / 60.0f;
    #else
    // Host code path
    h = std::fmod(h, 360.0f) / 60.0f;
    #endif
    
    int i = static_cast<int>(h);
    float f = h - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    
    float r, g, b;
    switch (i) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    
    Color result;
    result.r = static_cast<uint8_t>(r * 255.0f);
    result.g = static_cast<uint8_t>(g * 255.0f);
    result.b = static_cast<uint8_t>(b * 255.0f);
    result.a = 255;
    return result;
}

// Host/device implementation of Color::blend
__host__ __device__ Color Color::blend(const Color& c1, const Color& c2, float t) {
    // Use appropriate math functions depending on compilation context
    #if defined(__CUDA_ARCH__)
    // Device code path (inside CUDA kernel)
    t = fmaxf(0.0f, fminf(1.0f, t));
    #else
    // Host code path
    t = std::max(0.0f, std::min(1.0f, t));
    #endif
    
    Color result;
    result.r = static_cast<uint8_t>((1.0f - t) * c1.r + t * c2.r);
    result.g = static_cast<uint8_t>((1.0f - t) * c1.g + t * c2.g);
    result.b = static_cast<uint8_t>((1.0f - t) * c1.b + t * c2.b);
    result.a = static_cast<uint8_t>((1.0f - t) * c1.a + t * c2.a);
    return result;
}

// Update the structure for passing data between CPU and GPU
struct CudaRenderData {
    // Device pointers
    Color* d_frameBuffer;
    float* d_zBuffer;
    
    // Texture data on device
    struct TextureData {
        Color* pixels;
        int width;
        int height;
    };
    TextureData* d_textures;
    int numTextures;
    
    // Dimensions
    int width;
    int height;
};

// CUDA-compatible color blend function for device code
__device__ Color deviceBlendColor(const Color& c1, const Color& c2, float t) {
    // Use the Color::blend method directly - now it works in device code
    return Color::blend(c1, c2, t);
}

// CUDA-compatible HSV to RGB conversion for device code
__device__ Color deviceFromHSV(float h, float s, float v) {
    // Use the Color::fromHSV method directly - now it works in device code
    return Color::fromHSV(h, s, v);
}

__device__ void deviceSetPixel(Color* frameBuffer, int width, int height, int x, int y, const Color& color) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    
    const int index = y * width + x;
    
    // If alpha channel is fully opaque, just set the color
    if (color.a == 255) {
        frameBuffer[index] = color;
    }
    // Otherwise, blend with existing color
    else if (color.a > 0) {
        const float alpha = color.a / 255.0f;
        frameBuffer[index] = deviceBlendColor(frameBuffer[index], color, alpha);
    }
}

__device__ void deviceSetDepth(float* zBuffer, int width, int height, int x, int y, float depth) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    
    zBuffer[y * width + x] = depth;
}

__device__ float deviceGetDepth(const float* zBuffer, int width, int height, int x, int y) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return 1.0f;
    }
    
    return zBuffer[y * width + x];
}

__device__ bool deviceIsPixelVisible(const float* zBuffer, int width, int height, int x, int y, float depth) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return false;
    }
    
    return depth <= deviceGetDepth(zBuffer, width, height, x, y);
}

__device__ void deviceDrawPixelWithDepth(Color* frameBuffer, float* zBuffer, int width, int height, 
                                        int x, int y, float depth, const Color& color) {
    if (deviceIsPixelVisible(zBuffer, width, height, x, y, depth)) {
        deviceSetPixel(frameBuffer, width, height, x, y, color);
        deviceSetDepth(zBuffer, width, height, x, y, depth);
    }
}

// CUDA kernels

// Kernel for rendering skybox gradient
__global__ void skyboxGradientKernel(Color* frameBuffer, float* zBuffer, int width, int height,
                                  const Color zenithColor, const Color horizonColor, 
                                  float horizonY, bool performanceMode) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    // In performance mode, process only every other line
    if (performanceMode && (y % 2 != 0)) return;
    
    // Calculate factor for gradient (0 at horizon, 1 at top of screen)
    float t = 0.0f;
    if (y < horizonY) {
        t = 1.0f - (y / horizonY);
    }
    
    // Calculate gradient color using direct Color::blend method
    Color skyColor = Color::blend(horizonColor, zenithColor, t);
    
    // Set pixel and depth
    deviceSetPixel(frameBuffer, width, height, x, y, skyColor);
    deviceSetDepth(zBuffer, width, height, x, y, 1.0f);
    
    // In performance mode, also fill the next line if we're not at the bottom
    if (performanceMode && y + 1 < height && y + 1 < horizonY) {
        deviceSetPixel(frameBuffer, width, height, x, y + 1, skyColor);
        deviceSetDepth(zBuffer, width, height, x, y + 1, 1.0f);
    }
}

// Kernel for rendering the sun
__global__ void sunRenderKernel(Color* frameBuffer, float* zBuffer, int width, int height,
                             float screenX, float screenY, float radius, 
                             const Color sunColor, const Color glowColor,
                             float intensity, float glowSize, bool performanceMode) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    // Determine step size based on performance mode
    int step = performanceMode ? 2 : 1;
    
    // Skip pixels based on step size
    if (x % step != 0 || y % step != 0) return;
    
    // Calculate distance from sun center
    float dx = x - screenX;
    float dy = y - screenY;
    float distance = sqrtf(dx * dx + dy * dy);
    
    // Maximum size of the sun's glow effect
    float maxGlowRadius = radius * glowSize;
    
    // If within sun's glow radius
    if (distance <= maxGlowRadius) {
        // Calculate brightness based on distance
        float brightness = 0.0f;
        
        if (distance <= radius) {
            // Inside the sun - bright center fading to edge
            brightness = intensity * (1.0f - (distance / radius) * 0.2f);
        } else {
            // In the glow area - fade out with distance
            brightness = intensity * (1.0f - ((distance - radius) / (maxGlowRadius - radius)));
        }
        
        // Determine color based on whether we're in the sun or glow area
        Color baseColor = (distance <= radius) ? sunColor : glowColor;
        
        // Additive blending for glow effect
        Color currentColor = frameBuffer[y * width + x];
        uint8_t r = min(255, currentColor.r + static_cast<uint8_t>(baseColor.r * brightness));
        uint8_t g = min(255, currentColor.g + static_cast<uint8_t>(baseColor.g * brightness));
        uint8_t b = min(255, currentColor.b + static_cast<uint8_t>(baseColor.b * brightness));
        
        // Use direct initialization instead of constructor for CUDA compatibility
        Color newColor;
        newColor.r = r;
        newColor.g = g;
        newColor.b = b;
        newColor.a = 255;
        
        deviceSetPixel(frameBuffer, width, height, x, y, newColor);
        
        // In performance mode, fill adjacent pixels
        if (performanceMode) {
            for (int i = 0; i < step; i++) {
                for (int j = 0; j < step; j++) {
                    if (i == 0 && j == 0) continue; // Skip the pixel we already drew
                    
                    int nx = x + i;
                    int ny = y + j;
                    
                    if (nx < width && ny < height) {
                        deviceSetPixel(frameBuffer, width, height, nx, ny, newColor);
                    }
                }
            }
        }
    }
}

// Add a Wall collision data structure for the CUDA implementation
struct CudaWallCollision {
    bool collision;      // Whether a wall collision occurred
    float distance;      // Distance to the wall
    float wallHeight;    // Height of the wall
    int textureId;       // Texture ID for the wall
    float texCoordU;     // Texture U coordinate
    float floorHeight;   // Floor height of the sector
    float ceilingHeight; // Ceiling height of the sector
    bool isPortal;       // Whether the wall is a portal
    int lightLevel;      // Light level for the wall
};

// New BSP ray casting kernel
__global__ void bspRenderKernel(
    Color* frameBuffer,
    float* zBuffer,
    int width, 
    int height,
    float playerX,
    float playerY,
    float playerAngle,
    float playerHeight,
    float fov,
    float maxDistance,
    float floorLevel,
    float ceilingLevel)
{
    // Calculate the current pixel coordinates
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    
    // Early exit if outside screen bounds
    if (x >= width) return;
    
    // Calculate ray angle for this column
    float halfFov = fov * 0.5f * (PI / 180.0f);
    float angleStep = fov * (PI / 180.0f) / width;
    float rayAngle = playerAngle - halfFov + angleStep * x;
    
    // Normalize angle to [0, 2π)
    while (rayAngle < 0) rayAngle += 2 * PI;
    while (rayAngle >= 2 * PI) rayAngle -= 2 * PI;
    
    // Ray direction vector
    float rayDirX = cosf(rayAngle);
    float rayDirY = sinf(rayAngle);
    
    // Since we can't directly access the BSP tree in CUDA, we would need to
    // either upload a simplified version of the tree to GPU memory or
    // perform ray casting on the CPU and upload results.
    // For now, we'll simulate wall detection with a simple distance calculation
    // This should be replaced with actual BSP tree traversal in a future version
    
    // Placeholder: Simulating a wall detection at a fixed distance
    // In a real implementation, this would come from traversing the BSP tree
    CudaWallCollision collision;
    collision.collision = true;
    collision.distance = maxDistance * 0.5f;  // Simulate a wall halfway to max distance
    collision.wallHeight = 1.0f;  // Standard wall height
    collision.textureId = 1;      // Default texture
    collision.texCoordU = 0.5f;   // Middle of texture
    collision.floorHeight = floorLevel;
    collision.ceilingHeight = ceilingLevel + collision.wallHeight;
    collision.isPortal = false;
    collision.lightLevel = 255;  // Full brightness
    
    if (collision.collision) {
        // Correct for fisheye effect
        float correctedDistance = collision.distance * cosf(rayAngle - playerAngle);
        
        // Calculate projected wall height
        float distanceFactor = DISTANCE_MULTIPLIER / correctedDistance;
        float projectedWallHeight = collision.wallHeight * distanceFactor;
        
        // Calculate wall top and bottom screen positions
        float wallMidY = height / 2.0f;
        
        // Calculate vertical positions relative to player eye level
        float floorDiff = collision.floorHeight - playerHeight;
        float ceilingDiff = collision.ceilingHeight - playerHeight;
        
        // Project these differences to screen space
        float floorScreenY = wallMidY + floorDiff * distanceFactor;
        float ceilingScreenY = wallMidY + ceilingDiff * distanceFactor;
        
        // Ensure the wall is drawn within screen bounds
        int wallTop = max(0, (int)ceilingScreenY);
        int wallBottom = min(height - 1, (int)floorScreenY);
        
        // Simple coloring based on distance (placeholder for texture mapping)
        float intensityFactor = 1.0f - min(1.0f, correctedDistance / maxDistance);
        intensityFactor = max(0.2f, intensityFactor) * collision.lightLevel / 255.0f;
        
        // Draw the wall column
        for (int y = wallTop; y <= wallBottom; y++) {
            // Calculate texture coordinate V
            float wallY = (y - ceilingScreenY) / (floorScreenY - ceilingScreenY);
            
            // Simple color for now (would be texture sampling)
            Color wallColor = Color(
                (uint8_t)(200 * intensityFactor),
                (uint8_t)(200 * intensityFactor),
                (uint8_t)(200 * intensityFactor)
            );
            
            // Set pixel with depth
            int idx = y * width + x;
            frameBuffer[idx] = wallColor;
            zBuffer[idx] = correctedDistance / maxDistance;
        }
    }
}

// Floor and ceiling rendering kernel
__global__ void floorCeilingRenderKernel(
    Color* frameBuffer,
    float* zBuffer,
    int width,
    int height,
    float playerX,
    float playerY,
    float playerAngle,
    float playerHeight,
    float fov,
    float maxDistance,
    float floorHeight,
    float ceilingHeight)
{
    // Calculate the current pixel coordinates
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    // Early exit if outside screen bounds
    if (x >= width || y >= height) return;
    
    // Skip if pixel is in the middle section (already drawn by walls)
    int horizon = height / 2;
    bool isFloor = y > horizon;
    
    // Only process floor below horizon and ceiling above horizon
    if ((isFloor && y <= horizon) || (!isFloor && y >= horizon)) return;
    
    // Calculate ray angle for this column
    float halfFov = fov * 0.5f * (PI / 180.0f);
    float angleStep = fov * (PI / 180.0f) / width;
    float rayAngle = playerAngle - halfFov + angleStep * x;
    
    // Normalize angle to [0, 2π)
    while (rayAngle < 0) rayAngle += 2 * PI;
    while (rayAngle >= 2 * PI) rayAngle -= 2 * PI;
    
    // Ray direction vector
    float rayDirX = cosf(rayAngle);
    float rayDirY = sinf(rayAngle);
    
    // Calculate the vertical position factor relative to horizon
    float verticalAngle = 0.0f;
    if (isFloor) {
        // Floor rendering - calculate distance based on screen Y
        verticalAngle = (y - horizon) / (float)(height - horizon);
    } else {
        // Ceiling rendering - calculate distance based on screen Y
        verticalAngle = (horizon - y) / (float)horizon;
    }
    
    // Avoid division by zero
    verticalAngle = max(0.01f, verticalAngle);
    
    // Calculate the distance to the point on floor/ceiling
    float heightDiff = isFloor ? (playerHeight - floorHeight) : (ceilingHeight - playerHeight);
    float distance = heightDiff / verticalAngle * DISTANCE_MULTIPLIER / height;
    
    // If too far, don't render (fog)
    if (distance > maxDistance) return;
    
    // Calculate world position
    float worldX = playerX + rayDirX * distance;
    float worldY = playerY + rayDirY * distance;
    
    // Simple texture coordinates based on world position
    float texU = fmodf(worldX, 1.0f);
    float texV = fmodf(worldY, 1.0f);
    
    if (texU < 0) texU += 1.0f;
    if (texV < 0) texV += 1.0f;
    
    // Apply a checkerboard pattern for demonstration
    bool isEvenX = (int)worldX % 2 == 0;
    bool isEvenY = (int)worldY % 2 == 0;
    bool isCheckerLight = isEvenX != isEvenY;
    
    // Apply a distance fog effect
    float fogFactor = 1.0f - min(1.0f, distance / maxDistance);
    
    // Choose color based on floor/ceiling and checker pattern
    Color baseColor;
    if (isFloor) {
        baseColor = isCheckerLight ? Color(80, 80, 80) : Color(40, 40, 40);
    } else {
        baseColor = isCheckerLight ? Color(100, 100, 150) : Color(60, 60, 100);
    }
    
    // Apply fog effect
    Color finalColor = Color(
        (uint8_t)(baseColor.r * fogFactor),
        (uint8_t)(baseColor.g * fogFactor),
        (uint8_t)(baseColor.b * fogFactor)
    );
    
    // Set pixel with depth
    int idx = y * width + x;
    
    // Only draw if this point is closer than what's already there
    if (distance / maxDistance < zBuffer[idx]) {
        frameBuffer[idx] = finalColor;
        zBuffer[idx] = distance / maxDistance;
    }
}

// Structure to hold sprite rendering data for CUDA
struct CudaSpriteData {
    float x, y;           // Position in world space
    float scale;          // Sprite scale factor
    int textureId;        // Texture ID for the sprite
    int type;             // Sprite type
    float distance;       // Distance to the sprite from player
    bool visible;         // Whether the sprite is visible
};

// This kernel will be called once per sprite
__global__ void spriteRenderKernel(
    Color* frameBuffer,
    float* zBuffer,
    int width,
    int height,
    float playerX,
    float playerY,
    float playerAngle,
    float playerHeight,
    float fov,
    CudaSpriteData sprite,
    int textureWidth,
    int textureHeight,
    Color* textureData)  // We would need to pass texture data to the kernel
{
    // Calculate the current pixel coordinates
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    // Early exit if outside screen bounds or sprite not visible
    if (x >= width || y >= height || !sprite.visible) return;
    
    // Calculate direction to sprite from player
    float dx = sprite.x - playerX;
    float dy = sprite.y - playerY;
    
    // Calculate sprite angle relative to player's view
    float spriteAngle = atan2f(dy, dx);
    
    // Normalize angles to [0, 2π)
    while (spriteAngle < 0) spriteAngle += 2 * PI;
    while (spriteAngle >= 2 * PI) spriteAngle -= 2 * PI;
    while (playerAngle < 0) playerAngle += 2 * PI;
    while (playerAngle >= 2 * PI) playerAngle -= 2 * PI;
    
    // Calculate relative angle (accounting for wraparound)
    float relativeAngle = spriteAngle - playerAngle;
    if (relativeAngle > PI) relativeAngle -= 2 * PI;
    if (relativeAngle < -PI) relativeAngle += 2 * PI;
    
    // Check if sprite is in field of view
    float halfFovRadians = fov * 0.5f * DEG_TO_RAD;
    if (fabs(relativeAngle) > halfFovRadians) return;
    
    // Calculate screen position of sprite center
    float normalizedAngle = relativeAngle / halfFovRadians;  // [-1, 1]
    float screenX = (width / 2.0f) * (1.0f + normalizedAngle);
    
    // Calculate sprite size on screen
    float spriteSize = min(height, (int)(height / sprite.distance * sprite.scale * DISTANCE_MULTIPLIER / 120.0f));
    if (spriteSize <= 0) return;
    
    // Calculate sprite screen coordinates
    float halfSize = spriteSize / 2.0f;
    float spriteTopY = height / 2.0f - halfSize;
    float spriteBottomY = height / 2.0f + halfSize;
    float spriteLeftX = screenX - halfSize;
    float spriteRightX = screenX + halfSize;
    
    // Calculate this thread's contribution to the sprite
    float textureU = (x - spriteLeftX) / (spriteRightX - spriteLeftX);
    float textureV = (y - spriteTopY) / (spriteBottomY - spriteTopY);
    
    // Check if this pixel is within the sprite bounds
    if (textureU < 0.0f || textureU >= 1.0f || textureV < 0.0f || textureV >= 1.0f) return;
    
    // Sample the texture (simple nearest neighbor sampling)
    int texX = (int)(textureU * textureWidth);
    int texY = (int)(textureV * textureHeight);
    int texIndex = texY * textureWidth + texX;
    
    // Get the texel color
    Color texColor = textureData[texIndex];
    
    // Skip transparent pixels
    if (texColor.a < 10) return;
    
    // Apply distance-based fog
    float fogFactor = 1.0f - min(1.0f, sprite.distance / 30.0f);
    Color finalColor = Color(
        (uint8_t)(texColor.r * fogFactor),
        (uint8_t)(texColor.g * fogFactor),
        (uint8_t)(texColor.b * fogFactor),
        texColor.a
    );
    
    // Calculate the pixel index
    int idx = y * width + x;
    
    // Apply depth test - only draw if this sprite's pixel is closer than what's already drawn
    if (sprite.distance < zBuffer[idx] * 30.0f) {  // Convert normalized z-buffer to world distance
        // Handle alpha blending
        if (texColor.a < 255) {
            // Blend with existing color
            float alpha = texColor.a / 255.0f;
            Color existingColor = frameBuffer[idx];
            finalColor = Color(
                (uint8_t)(existingColor.r * (1.0f - alpha) + finalColor.r * alpha),
                (uint8_t)(existingColor.g * (1.0f - alpha) + finalColor.g * alpha),
                (uint8_t)(existingColor.b * (1.0f - alpha) + finalColor.b * alpha),
                255
            );
        }
        
        frameBuffer[idx] = finalColor;
        // Update z-buffer with a slight bias for sprites (0.99) to prevent z-fighting
        zBuffer[idx] = sprite.distance / 30.0f * 0.99f;
    }
}

// Add a new CUDA kernel for clearing the Z-buffer
__global__ void clearZBufferKernel(float* zBuffer, int width, int height, float clearValue) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x < width && y < height) {
        int idx = y * width + x;
        zBuffer[idx] = clearValue;
    }
}

// Host code for RendererCuda implementation

RendererCuda::RendererCuda(int width, int height) 
    : m_width(width), m_height(height), m_cudaAvailable(false), m_initialized(false), 
      m_buffersAllocated(false), m_texturesUploaded(false), m_cudaData(nullptr) {
    // Initialize skybox with default values
    m_skybox.maxViewDistance = 30.0f; // Default max view distance
}

RendererCuda::~RendererCuda() {
    if (m_initialized) {
        freeCudaMemory();
    }
    
    if (m_cudaData) {
        delete m_cudaData;
    }
    
    if (m_cudaAvailable) {
        cleanupCuda();
    }
}

bool RendererCuda::initialize() {
    // Initialize CUDA
    m_deviceInfo = initializeCuda();
    m_cudaAvailable = m_deviceInfo.cudaAvailable;
    
    if (!m_cudaAvailable) {
        std::cerr << "CUDA initialization failed. Using CPU fallback." << std::endl;
        return false;
    }
    
    // Print device info
    printCudaDeviceInfo(m_deviceInfo);
    
    // Allocate CUDA data structure
    m_cudaData = new CudaRenderData();
    m_cudaData->width = m_width;
    m_cudaData->height = m_height;
    
    // Allocate device memory
    allocateCudaMemory();
    
    m_initialized = true;
    return true;
}

void RendererCuda::allocateBuffers() {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData || m_buffersAllocated) {
        return;  // Already allocated or can't allocate
    }
    
    // Allocate device memory for frame buffer
    CUDA_CHECK(cudaMalloc(&m_cudaData->d_frameBuffer, m_width * m_height * sizeof(Color)));
    
    // Allocate device memory for Z-buffer
    CUDA_CHECK(cudaMalloc(&m_cudaData->d_zBuffer, m_width * m_height * sizeof(float)));
    
    m_buffersAllocated = true;
    
    // Clear the newly allocated buffers
    clearBuffers();
}

void RendererCuda::freeBuffers() {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData || !m_buffersAllocated) {
        return;  // Not allocated or can't free
    }
    
    // Free device memory for frame buffer
    if (m_cudaData->d_frameBuffer) {
        CUDA_CHECK(cudaFree(m_cudaData->d_frameBuffer));
        m_cudaData->d_frameBuffer = nullptr;
    }
    
    // Free device memory for Z-buffer
    if (m_cudaData->d_zBuffer) {
        CUDA_CHECK(cudaFree(m_cudaData->d_zBuffer));
        m_cudaData->d_zBuffer = nullptr;
    }
    
    m_buffersAllocated = false;
}

void RendererCuda::clearBuffers() {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData || !m_buffersAllocated) {
        return;  // Not allocated or can't clear
    }
    
    // Clear frame buffer to black
    CUDA_CHECK(cudaMemset(m_cudaData->d_frameBuffer, 0, m_width * m_height * sizeof(Color)));
    
    // Set Z-buffer to far distance (1.0f) using a kernel
    dim3 blockSize(16, 16);
    dim3 gridSize((m_width + blockSize.x - 1) / blockSize.x,
                 (m_height + blockSize.y - 1) / blockSize.y);
    
    clearZBufferKernel<<<gridSize, blockSize>>>(
        m_cudaData->d_zBuffer,
        m_width,
        m_height,
        1.0f  // Far distance
    );
    
    // Check for errors
    CUDA_CHECK(cudaGetLastError());
}

void RendererCuda::allocateCudaMemory() {
    if (!m_cudaAvailable || !m_cudaData) return;
    
    // Allocate initial buffers
    allocateBuffers();
    
    // Initialize texture data array
    m_cudaData->numTextures = 0;
    m_cudaData->d_textures = nullptr;
}

void RendererCuda::freeCudaMemory() {
    if (!m_cudaAvailable || !m_cudaData) return;
    
    // Free buffer memory
    freeBuffers();
    
    // Free texture data
    if (m_cudaData->d_textures) {
        // Free each texture's pixel data
        for (int i = 0; i < m_cudaData->numTextures; ++i) {
            if (m_cudaData->d_textures[i].pixels) {
                CUDA_CHECK(cudaFree(m_cudaData->d_textures[i].pixels));
            }
        }
        
        // Free the texture array
        CUDA_CHECK(cudaFree(m_cudaData->d_textures));
        m_cudaData->d_textures = nullptr;
        m_cudaData->numTextures = 0;
        m_texturesUploaded = false;
    }
}

void RendererCuda::renderFrame(const BSPTree& bsp, const ViewPosition& view,
                            const std::vector<Sprite>& sprites, float deltaTime) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    // Make sure buffers are allocated
    if (!m_buffersAllocated) {
        allocateBuffers();
    }
    
    // Clear buffers for new frame
    clearBuffers();
    
    // Render all components directly on the GPU
    renderSkyboxCuda(view, deltaTime, m_skybox);
    renderBSPCuda(bsp, view, m_skybox.maxViewDistance);
    renderFloorAndCeilingCuda(bsp, view);
    renderSpritesCuda(bsp, view, sprites);
    
    // No need to synchronize - retrieveRenderingResults will wait for kernels to finish
}

void RendererCuda::renderSkyboxCuda(const ViewPosition& view, float deltaTime, const Skybox& skybox) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    // Calculate horizon position (scaled to screen height)
    float horizonY = m_height / 2.0f;
    
    // Determine block and grid sizes
    dim3 blockSize = getOptimalBlockSize(m_width, m_height);
    dim3 gridSize((m_width + blockSize.x - 1) / blockSize.x, 
                 (m_height + blockSize.y - 1) / blockSize.y);
    
    // Check if we're in performance mode
    bool performanceMode = skybox.maxViewDistance < 20.0f;
    
    // Launch the skybox gradient kernel
    skyboxGradientKernel<<<gridSize, blockSize>>>(
        m_cudaData->d_frameBuffer,
        m_cudaData->d_zBuffer,
        m_width,
        m_height,
        skybox.zenithColor,
        skybox.horizonColor,
        horizonY,
        performanceMode
    );
    
    // Check for errors
    CUDA_CHECK(cudaGetLastError());
    
    // Draw the sun if dynamic sky is enabled
    if (skybox.dynamicSky) {
        // Calculate sun position on screen
        float sunScreenAngle = skybox.sunAngle - view.angle;
        
        // Normalize angle to [-PI, PI]
        while (sunScreenAngle > PI) sunScreenAngle -= 2.0f * PI;
        while (sunScreenAngle < -PI) sunScreenAngle += 2.0f * PI;
        
        // Check if sun is visible (within field of view)
        float halfFov = view.fov * DEG_TO_RAD / 2.0f;
        if (sunScreenAngle >= -halfFov && sunScreenAngle <= halfFov) {
            // Calculate screen position
            float screenX = m_width / 2.0f + m_width * (sunScreenAngle / (view.fov * DEG_TO_RAD)) * 0.5f;
            float screenY = m_height / 2.0f - m_height * skybox.sunHeight * 0.5f;
            
            // Sun radius in pixels
            float sunRadius = skybox.sunSize * m_width / (view.fov * 2.0f);
            
            // Draw the sun
            drawSunCuda(screenX, screenY, sunRadius, skybox.sunColor, 1.0f, skybox);
        }
    }
    
    // Synchronize to ensure all kernels complete
    CUDA_CHECK(cudaDeviceSynchronize());
}

void RendererCuda::drawSunCuda(float screenX, float screenY, float radius, const Color& color, 
                            float intensity, const Skybox& skybox) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    // Determine block and grid sizes to cover the sun area
    dim3 blockSize = getOptimalBlockSize(m_width, m_height);
    dim3 gridSize((m_width + blockSize.x - 1) / blockSize.x, 
                 (m_height + blockSize.y - 1) / blockSize.y);
    
    // Check if we're in performance mode
    bool performanceMode = skybox.maxViewDistance < 20.0f;
    
    // Launch the sun rendering kernel
    sunRenderKernel<<<gridSize, blockSize>>>(
        m_cudaData->d_frameBuffer,
        m_cudaData->d_zBuffer,
        m_width,
        m_height,
        screenX,
        screenY,
        radius,
        color,
        skybox.sunGlowColor,
        intensity,
        skybox.sunGlowSize,
        performanceMode
    );
    
    // Check for errors
    CUDA_CHECK(cudaGetLastError());
}

// Update the renderBSPCuda method from a stub to a real implementation
void RendererCuda::renderBSPCuda(const BSPTree& bsp, const ViewPosition& view, float maxViewDistance) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    // Get player information
    float playerX = view.position.x;
    float playerY = view.position.y;
    float playerAngle = view.angle;
    float playerHeight = view.height;
    float fov = view.fov;
    
    // Get current sector information (floor and ceiling heights)
    float floorLevel = 0.0f;
    float ceilingLevel = 0.0f;
    int playerSectorId = bsp.findSector(view.position);
    if (playerSectorId >= 0 && playerSectorId < static_cast<int>(bsp.getSectors().size())) {
        const Sector& playerSector = bsp.getSectors()[playerSectorId];
        floorLevel = playerSector.floorHeight;
        ceilingLevel = playerSector.ceilingHeight;
    }
    
    // Determine thread block and grid sizes
    dim3 blockSize(16, 1);  // Use 16 threads per block for simplicity
    dim3 gridSize((m_width + blockSize.x - 1) / blockSize.x);
    
    // Launch the BSP rendering kernel
    bspRenderKernel<<<gridSize, blockSize>>>(
        m_cudaData->d_frameBuffer,
        m_cudaData->d_zBuffer,
        m_width,
        m_height,
        playerX,
        playerY,
        playerAngle,
        playerHeight,
        fov,
        maxViewDistance,
        floorLevel,
        ceilingLevel
    );
    
    // Check for errors
    CUDA_CHECK(cudaGetLastError());
}

void RendererCuda::renderFloorAndCeilingCuda(const BSPTree& bsp, const ViewPosition& view) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    // Get player information
    float playerX = view.position.x;
    float playerY = view.position.y;
    float playerAngle = view.angle;
    float playerHeight = view.height;
    float fov = view.fov;
    
    // Get current sector information (floor and ceiling heights)
    float floorHeight = 0.0f;
    float ceilingHeight = 2.0f;  // Default ceiling height
    int playerSectorId = bsp.findSector(view.position);
    if (playerSectorId >= 0 && playerSectorId < static_cast<int>(bsp.getSectors().size())) {
        const Sector& playerSector = bsp.getSectors()[playerSectorId];
        floorHeight = playerSector.floorHeight;
        ceilingHeight = playerSector.ceilingHeight;
    }
    
    // Use the maxViewDistance from skybox
    float maxViewDistance = m_skybox.maxViewDistance;
    
    // Determine thread block and grid sizes - use 2D grid for floor/ceiling
    dim3 blockSize(16, 16);  // 16x16 threads per block
    dim3 gridSize(
        (m_width + blockSize.x - 1) / blockSize.x,
        (m_height + blockSize.y - 1) / blockSize.y
    );
    
    // Launch the floor and ceiling rendering kernel
    floorCeilingRenderKernel<<<gridSize, blockSize>>>(
        m_cudaData->d_frameBuffer,
        m_cudaData->d_zBuffer,
        m_width,
        m_height,
        playerX,
        playerY,
        playerAngle,
        playerHeight,
        fov,
        maxViewDistance,
        floorHeight,
        ceilingHeight
    );
    
    // Check for errors
    CUDA_CHECK(cudaGetLastError());
}

void RendererCuda::renderSpritesCuda(const BSPTree& bsp, const ViewPosition& view, 
                                   const std::vector<Sprite>& sprites) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData || sprites.empty()) return;
    
    // Get player information
    float playerX = view.position.x;
    float playerY = view.position.y;
    float playerAngle = view.angle;
    float playerHeight = view.height;
    float fov = view.fov;
    
    // Sort sprites by distance (farthest to nearest)
    std::vector<std::pair<float, size_t>> sortedIndices;
    for (size_t i = 0; i < sprites.size(); ++i) {
        const Sprite& sprite = sprites[i];
        
        // Calculate distance to sprite
        float dx = sprite.position.x - playerX;
        float dy = sprite.position.y - playerY;
        float distance = std::sqrt(dx * dx + dy * dy);
        
        // Add to the list of indices to sort
        sortedIndices.push_back(std::make_pair(distance, i));
    }
    
    // Sort from farthest to nearest
    std::sort(sortedIndices.begin(), sortedIndices.end(), 
              [](const std::pair<float, size_t>& a, const std::pair<float, size_t>& b) { 
                  return a.first > b.first; 
              });
    
    // Iterate through sorted sprites
    for (size_t i = 0; i < sortedIndices.size(); ++i) {
        float distance = sortedIndices[i].first;
        size_t index = sortedIndices[i].second;
        const Sprite& sprite = sprites[index];
        
        // Skip if too far or too close
        if (distance > 30.0f || distance < 0.1f) continue;
        
        // Get the current frame
        const SpriteFrame& frame = sprite.getCurrentFrame();
        int textureId = frame.textureId;
        
        // Check if the texture ID is valid
        if (textureId < 0 || textureId >= m_cudaData->numTextures) {
            std::cerr << "Invalid texture ID for sprite: " << textureId << std::endl;
            continue;
        }
        
        // Create sprite data for CUDA
        CudaSpriteData spriteData;
        spriteData.x = sprite.position.x;
        spriteData.y = sprite.position.y;
        spriteData.scale = sprite.scale;
        spriteData.textureId = textureId;
        spriteData.type = static_cast<int>(sprite.type);
        spriteData.distance = distance;
        spriteData.visible = sprite.visible;
        
        // Determine block and grid sizes for the sprite
        dim3 blockSize(16, 16);
        dim3 gridSize((m_width + blockSize.x - 1) / blockSize.x,
                      (m_height + blockSize.y - 1) / blockSize.y);
        
        // Get texture information
        CudaRenderData::TextureData textureData;
        CUDA_CHECK(cudaMemcpy(&textureData, 
                            &(m_cudaData->d_textures[textureId]), 
                            sizeof(CudaRenderData::TextureData), 
                            cudaMemcpyDeviceToHost));
        
        // Launch kernel for this sprite
        spriteRenderKernel<<<gridSize, blockSize>>>(
            m_cudaData->d_frameBuffer,
            m_cudaData->d_zBuffer,
            m_width,
            m_height,
            playerX,
            playerY, 
            playerAngle,
            playerHeight,
            fov,
            spriteData,
            textureData.width,
            textureData.height,
            textureData.pixels
        );
        
        // Check for errors
        CUDA_CHECK(cudaGetLastError());
    }
}

// Add a new method to upload textures to the GPU
void RendererCuda::uploadTextures(const std::vector<Texture>& textures) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    // If textures are already uploaded, free existing memory first
    if (m_texturesUploaded && m_cudaData->d_textures) {
        // Free each texture's pixel data
        for (int i = 0; i < m_cudaData->numTextures; ++i) {
            if (m_cudaData->d_textures[i].pixels) {
                CUDA_CHECK(cudaFree(m_cudaData->d_textures[i].pixels));
            }
        }
        
        // Free the texture array
        CUDA_CHECK(cudaFree(m_cudaData->d_textures));
        m_cudaData->d_textures = nullptr;
    }
    
    // Allocate new texture array
    int numTextures = static_cast<int>(textures.size());
    m_cudaData->numTextures = numTextures;
    
    if (numTextures > 0) {
        // Allocate device memory for texture array
        CUDA_CHECK(cudaMalloc(&m_cudaData->d_textures, numTextures * sizeof(CudaRenderData::TextureData)));
        
        // Create a host-side copy of the texture array
        CudaRenderData::TextureData* hostTextures = new CudaRenderData::TextureData[numTextures];
        
        // Upload each texture
        for (int i = 0; i < numTextures; ++i) {
            const Texture& texture = textures[i];
            int pixelCount = texture.width() * texture.height();
            
            // Allocate device memory for texture pixels
            CUDA_CHECK(cudaMalloc(&hostTextures[i].pixels, pixelCount * sizeof(Color)));
            
            // Copy texture data to device
            CUDA_CHECK(cudaMemcpy(hostTextures[i].pixels, texture.m_pixels.data(), 
                              pixelCount * sizeof(Color), cudaMemcpyHostToDevice));
            
            // Set texture dimensions
            hostTextures[i].width = texture.width();
            hostTextures[i].height = texture.height();
        }
        
        // Copy the texture array to device
        CUDA_CHECK(cudaMemcpy(m_cudaData->d_textures, hostTextures, 
                           numTextures * sizeof(CudaRenderData::TextureData), cudaMemcpyHostToDevice));
        
        // Free host-side array
        delete[] hostTextures;
        
        m_texturesUploaded = true;
    }
}

// Add these methods after the renderFrame method

void RendererCuda::prepareForRendering(const std::vector<Color>& frameBuffer, const std::vector<float>& zBuffer) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;

    // Make sure buffers are allocated
    if (!m_buffersAllocated) {
        allocateBuffers();
    } else {
        // Copy frame buffer data to device
        CUDA_CHECK(cudaMemcpy(m_cudaData->d_frameBuffer, frameBuffer.data(), 
                          m_width * m_height * sizeof(Color), cudaMemcpyHostToDevice));
        
        // Copy Z-buffer data to device
        CUDA_CHECK(cudaMemcpy(m_cudaData->d_zBuffer, zBuffer.data(), 
                          m_width * m_height * sizeof(float), cudaMemcpyHostToDevice));
    }
}

void RendererCuda::retrieveRenderingResults(std::vector<Color>& frameBuffer, std::vector<float>& zBuffer) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData || !m_buffersAllocated) return;
    
    // Copy frame buffer data back to host
    CUDA_CHECK(cudaMemcpy(frameBuffer.data(), m_cudaData->d_frameBuffer, 
                       m_width * m_height * sizeof(Color), cudaMemcpyDeviceToHost));
    
    // Copy Z-buffer data back to host
    CUDA_CHECK(cudaMemcpy(zBuffer.data(), m_cudaData->d_zBuffer, 
                       m_width * m_height * sizeof(float), cudaMemcpyDeviceToHost));
}

} // namespace PureDoom 