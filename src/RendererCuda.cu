#include "RendererCuda.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <iostream>
#include <cmath>      // For std::fmod
#include <algorithm>  // For std::max, std::min
#include <vector>     // For BSP serialization
#include <mutex>      // For std::mutex
#include <set>
#include <queue>
#include <cfloat>     // For FLT_MAX
#include "CudaTestMap.h" // Include the test map header
#include "Platform.h"    // Include Platform header

// Host and device implementations of Color methods for CUDA kernels
namespace PureDoom {

// Define data structure for CUDA rendering
struct CudaRenderData {
    // Frame buffer info
    Color* d_frameBuffer;
    float* d_zBuffer;
    int width;
    int height;
    
    // Texture data
    struct TextureData {
        Color* pixels;
        int width;
        int height;
    };
    
    TextureData* d_textures;
    int numTextures;
    
    // Current view information (for ray casting)
    ViewPosition currentView;
    
    // BSP tree data
    CudaBSPTree* d_bspTree;
};

// Structure to represent a platform in CUDA
struct CudaPlatform {
    float vertices[8][2];  // Up to 8 vertices (x,y coordinates)
    int vertexCount;       // Number of vertices
    float height;          // Height above the floor
    float thickness;       // Thickness of the platform
    int topTextureId;      // Texture ID for the top surface
    int bottomTextureId;   // Texture ID for the bottom surface
    int sideTextureId;     // Texture ID for the sides
    int lightLevel;        // Light level
    int sectorId;          // Sector ID
};

// Forward declaration of the platform rendering kernel
__global__ void renderPlatformsKernel(
    Color* frameBuffer,
    float* zBuffer,
    int width,
    int height,
    CudaPlatform* platforms,
    int platformCount,
    const ViewPosition view,
    CudaRenderData::TextureData* textures
);

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
    // Calculate the current pixel coordinates
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    // Early exit if outside screen bounds
    if (x >= width || y >= height) return;
    
    // Skip if below horizon (floor will handle that part)
    if (y >= horizonY) return;
    
    // Create a DOOM-like sky with horizontal bands
    Color skyColor;
    
    // Calculate gradient factor (0 at horizon, 1 at top of screen)
    float factor = 1.0f - (y / horizonY);
    
    // Color the sky with a DOOM-like gradient of purplish mountain silhouette
    if (factor < 0.15f) {
        // Mountain silhouette at the horizon - dark purplish gray
        skyColor = Color(35, 30, 50, 255);
    } else if (factor < 0.3f) {
        // Transition to medium purple
        float localFactor = (factor - 0.15f) / 0.15f;
        skyColor = Color(
            (uint8_t)(35 + localFactor * 45),  // 35 to 80
            (uint8_t)(30 + localFactor * 30),  // 30 to 60
            (uint8_t)(50 + localFactor * 50),  // 50 to 100
            255
        );
    } else if (factor < 0.55f) {
        // Mid-sky reddish
        float localFactor = (factor - 0.3f) / 0.25f;
        skyColor = Color(
            (uint8_t)(80 + localFactor * 120),  // 80 to 200
            (uint8_t)(60 + localFactor * 40),   // 60 to 100
            (uint8_t)(100 + localFactor * 10),  // 100 to 110
            255
        );
    } else {
        // Upper sky - transition to dark
        float localFactor = (factor - 0.55f) / 0.45f;
        skyColor = Color(
            (uint8_t)(200 - localFactor * 150),  // 200 to 50
            (uint8_t)(100 - localFactor * 70),   // 100 to 30
            (uint8_t)(110 - localFactor * 40),   // 110 to 70
            255
        );
    }
    
    // Set pixel directly with 1.0 depth (farthest)
    int idx = y * width + x;
    frameBuffer[idx] = skyColor;
    zBuffer[idx] = 1.0f;  // Maximum depth
}

// Kernel for casting shadow rays to determine light and shadow areas
__global__ void shadowRayCastKernel(int* wallHitBuffer, 
                                  float playerX, float playerY,
                                  float sunAngle, float sunHeight,
                                  const CudaBSPTree bspTree,
                                  float maxRayLength,
                                  int numRays) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= numRays) return;
    
    // Calculate ray direction based on ray angle
    float rayAngle = (2.0f * PI * idx) / numRays;
    
    // Rotate around sun position
    float rayDirX = cosf(sunAngle + rayAngle);
    float rayDirY = sinf(sunAngle + rayAngle);
    
    // Cast ray from player position
    CudaVec2 rayOrigin(playerX, playerY);
    CudaVec2 rayDir(rayDirX, rayDirY);
    
    // Cast ray and check for collision
    CudaWallCollision collision = castRayBSP(bspTree, rayOrigin, rayDir, maxRayLength);
    
    // Store the normalized distance (0-1) if there's a hit, or 0 if no hit
    if (collision.collision) {
        // We hit something, store normalized distance
        float normDistance = collision.distance / maxRayLength;
        wallHitBuffer[idx] = __float_as_int(normDistance);
    } else {
        // No collision, set to 0
        wallHitBuffer[idx] = __float_as_int(0.0f);
    }
}

// New kernel for applying directional light to map surfaces
__global__ void applyDirectionalLightKernel(
    Color* frameBuffer,
    float* zBuffer,
    int width, 
    int height,
    float sunAngle,
    float sunHeight,
    float sunX,
    float sunY,
    float playerX,
    float playerY,
    float playerAngle,
    const Color sunColor,
    float rayIntensity,
    float* customParams,
    int* wallHitBuffer,
    int numRays,
    float maxViewDistance
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    // Only process pixels with valid depth (not skybox)
    int pixelIndex = y * width + x;
    float depth = zBuffer[pixelIndex];
    if (depth >= 0.999f) return; // Skip skybox pixels
    
    // Get custom parameters
    bool enableLighting = customParams[0] > 0.5f;
    float mapLightingIntensity = customParams[8]; // New parameter - intensity of light on map
    float lightAttenuation = customParams[9]; // New parameter - distance attenuation
    bool enableShadowCasting = customParams[5] > 0.5f;
    float shadowIntensity = customParams[6];
    float shadowSoftness = customParams[7];
    
    if (!enableLighting) return;
    
    // Convert screen coordinates to world ray direction
    float normalizedX = (float)x / width - 0.5f;
    float normalizedY = (float)y / height - 0.5f;
    
    // Calculate ray direction in world space
    float fovRadians = PI / 2.0f; // 90 degrees in radians
    float rayAngleX = playerAngle + normalizedX * fovRadians;
    
    // Calculate rough world position based on depth
    float worldDistance = maxViewDistance * depth;
    float worldX = playerX + cosf(rayAngleX) * worldDistance;
    float worldY = playerY + sinf(rayAngleX) * worldDistance;
    
    // Calculate direction from world position to sun
    float toSunX = cosf(sunAngle);
    float toSunY = sinf(sunAngle);
    
    // Calculate light contribution by checking if this point is in shadow
    float lightFactor = 0.0f;
    
    // Determine which ray direction is closest to this pixel's direction to sun
    float bestAngle = 2.0f * PI;
    int bestRayIdx = 0;
    
    // Calculate angle to sun from this world position
    float angleToSun = atan2f(toSunY, toSunX);
    if (angleToSun < 0) angleToSun += 2.0f * PI;
    
    // Find the closest ray direction
    for (int i = 0; i < numRays; i++) {
        float rayAngle = (2.0f * PI * i) / numRays;
        float angleDiff = fabsf(rayAngle - angleToSun);
        if (angleDiff > PI) angleDiff = 2.0f * PI - angleDiff;
        
        if (angleDiff < bestAngle) {
            bestAngle = angleDiff;
            bestRayIdx = i;
        }
    }
    
    // Check if point is in shadow using the closest ray
    float rayHitDistance = __int_as_float(wallHitBuffer[bestRayIdx]);
    
    // Calculate dot product between surface normal and light direction
    // (For simplicity, estimate normal as pointing away from player)
    float normalX = worldX - playerX;
    float normalY = worldY - playerY;
    float normalLength = sqrtf(normalX*normalX + normalY*normalY);
    if (normalLength > 0.001f) {
        normalX /= normalLength;
        normalY /= normalLength;
    }
    
    float dotProduct = normalX * toSunX + normalY * toSunY;
    float lightAngleFactor = max(0.0f, dotProduct); // Cosine falloff based on normal
    
    // Calculate distance from this point to player
    float distToPlayer = worldDistance;
    
    // Distance attenuation (inverse square falloff)
    float distanceAttenuation = 1.0f / (1.0f + distToPlayer * lightAttenuation);
    
    // Shadow check
    bool inShadow = false;
    if (enableShadowCasting && rayHitDistance > 0.0f) {
        // Convert world position to normalized distance along ray
        float distAlongRay = sqrtf((worldX - playerX)*(worldX - playerX) + (worldY - playerY)*(worldY - playerY)) / maxViewDistance;
        
        // If this point is beyond where the ray hit something, it's in shadow
        if (distAlongRay > rayHitDistance) {
            inShadow = true;
            
            // Apply soft shadow based on distance beyond hit point
            float shadowFactor = min(1.0f, (distAlongRay - rayHitDistance) / shadowSoftness);
            lightFactor = max(0.0f, (1.0f - shadowFactor * shadowIntensity)) * lightAngleFactor * distanceAttenuation;
        } else {
            // Not in shadow, full light
            lightFactor = lightAngleFactor * distanceAttenuation;
        }
    } else {
        // No shadow casting or no hit, full light
        lightFactor = lightAngleFactor * distanceAttenuation;
    }
    
    // Apply map lighting intensity to intensify the effect
    lightFactor *= mapLightingIntensity;
    
    // Apply light factor to pixel color
    Color pixelColor = frameBuffer[pixelIndex];
    
    // Base light level (minimum ambient light)
    float ambientLevel = 0.3f;
    float totalLight = ambientLevel + (1.0f - ambientLevel) * lightFactor * rayIntensity;
    
    // Create sunlight color influence
    Color litColor;
    // Blend with sun color based on light factor
    float sunInfluence = lightFactor * 0.6f; // Control how much the sun color affects the scene
    
    // Add a subtle pulsing effect to the light (similar to the rays)
    float pulseEffect = 0.0f;
    if (inShadow) {
        // No pulse in shadow
        pulseEffect = 0.0f;
    } else {
        // Pulse based on distance and angle
        pulseEffect = 0.15f * sinf(distToPlayer * 0.5f + angleToSun * 8.0f);
    }
    
    // Apply the lighting with pulse effect
    litColor.r = (uint8_t)min(255.0f, pixelColor.r * (1.0f - sunInfluence) + 
                          sunColor.r * sunInfluence * (totalLight + pulseEffect));
    litColor.g = (uint8_t)min(255.0f, pixelColor.g * (1.0f - sunInfluence) + 
                          sunColor.g * sunInfluence * (totalLight + pulseEffect));
    litColor.b = (uint8_t)min(255.0f, pixelColor.b * (1.0f - sunInfluence) + 
                          sunColor.b * sunInfluence * (totalLight + pulseEffect));
    litColor.a = pixelColor.a;
    
    // Write final pixel
    frameBuffer[pixelIndex] = litColor;
}

// Kernel for rendering the sun with light rays
__global__ void sunRenderKernel(Color* frameBuffer, float* zBuffer, int width, int height,
                             float screenX, float screenY, float radius, 
                             const Color sunColor, const Color glowColor,
                             float intensity, float glowSize, bool performanceMode,
                             float* customParams, int* wallHitBuffer) {
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
    
    // Get custom parameters
    bool enableHellSun = customParams[0] > 0.5f;
    int numLightRays = (int)customParams[1];
    float rayLength = customParams[2];
    float rayWidth = customParams[3];
    float rayIntensity = customParams[4];
    bool enableShadowCasting = customParams[5] > 0.5f;
    float shadowIntensity = customParams[6];
    float shadowSoftness = customParams[7];
    
    // Check if this pixel is inside a light ray
    bool isInLightRay = false;
    float rayBrightness = 0.0f;
    
    if (enableHellSun && numLightRays > 0) {
        // Maximum ray distance from sun center
        float maxRayDistance = radius * rayLength;
        
        // Calculate angle from sun center to current pixel
        float angle = atan2f(dy, dx);
        if (angle < 0) angle += 2.0f * PI;
        while (angle < 0) angle += 2.0f * PI;
        while (angle > 2.0f * PI) angle -= 2.0f * PI;
        
        // Angle step between rays
        float rayAngleStep = 2.0f * PI / numLightRays;
        
        // Check if the current pixel falls within any light ray
        for (int i = 0; i < numLightRays; i++) {
            float rayAngle = i * rayAngleStep;
            
            // Calculate angular distance from ray center
            float angleDiff = fabsf(angle - rayAngle);
            if (angleDiff > PI) angleDiff = 2.0f * PI - angleDiff;
            
            // Ray width varies with distance from sun
            float currentRayWidth = rayWidth * (1.0f + distance / maxRayDistance * 0.5f);
            
            // Check if pixel is within a light ray
            if (angleDiff < currentRayWidth && distance > radius && distance < maxRayDistance) {
                // Calculate ray brightness based on distance from center of ray
                float rayFactor = 1.0f - (angleDiff / currentRayWidth);
                
                // Ray intensity decreases with distance from sun
                float distanceFactor = 1.0f - ((distance - radius) / (maxRayDistance - radius));
                
                // Ray intensity also has a pulsating effect
                float pulseFactor = 0.7f + 0.3f * sinf(distanceFactor * 8.0f);
                
                rayBrightness = rayFactor * distanceFactor * pulseFactor * rayIntensity;
                isInLightRay = true;
                
                // Shadow casting
                if (enableShadowCasting && wallHitBuffer != nullptr) {
                    // Get the distance to the nearest wall along this ray
                    int rayIndex = i % numLightRays;
                    float wallHitDistance = __int_as_float(wallHitBuffer[rayIndex]);
                    
                    // If there's a wall hit and we're beyond it, apply shadow
                    if (wallHitDistance > 0.0f && distance > (radius + wallHitDistance * maxRayDistance)) {
                        // Calculate shadow intensity with soft edges
                        float shadowFactor = min(1.0f, (distance - (radius + wallHitDistance * maxRayDistance)) 
                                               / (maxRayDistance * shadowSoftness));
                        
                        // Apply shadow darkening
                        rayBrightness *= (1.0f - shadowFactor * shadowIntensity);
                    }
                }
                
                break;
            }
        }
    }
    
    // If within sun's glow radius or in a light ray
    if (distance <= maxGlowRadius || isInLightRay) {
        // Calculate brightness based on distance
        float brightness = 0.0f;
        
        if (distance <= radius) {
            // Inside the sun - bright center fading to edge
            brightness = intensity * (1.0f - (distance / radius) * 0.2f);
        } else if (!isInLightRay) {
            // In the glow area - fade out with distance
            brightness = intensity * (1.0f - ((distance - radius) / (maxGlowRadius - radius)));
        } else {
            // In a light ray
            brightness = rayBrightness;
        }
        
        // Determine color based on whether we're in the sun, glow area, or light ray
        Color baseColor;
        if (distance <= radius) {
            baseColor = sunColor;
        } else if (isInLightRay) {
            // Light rays are more orange-red
            baseColor = Color(255, 100 + (int)(80.0f * rayBrightness), 20 + (int)(30.0f * rayBrightness));
        } else {
            baseColor = glowColor;
        }
        
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

// Update BSP ray casting kernel to use serialized BSP tree and textures
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
    CudaBSPTree* bspTree,
    CudaRenderData::TextureData* textures,
    int numTextures)
{
    // Calculate the current pixel coordinates
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    
    // Early exit if outside screen bounds
    if (x >= width) return;
    
    // Early exit if no BSP tree is available
    if (bspTree == nullptr || bspTree->nodes == nullptr || 
        bspTree->walls == nullptr || bspTree->sectors == nullptr) {
        return;
    }

    // Calculate ray angle for this column - use more precise angle calculation
    float halfFov = fov * 0.5f * (PI / 180.0f);
    float angleStep = fov * (PI / 180.0f) / width;
    float rayAngle = playerAngle - halfFov + angleStep * x;
    
    // Normalize angle to [0, 2π)
    while (rayAngle < 0) rayAngle += 2 * PI;
    while (rayAngle >= 2 * PI) rayAngle -= 2 * PI;
    
    // Calculate ray direction with more precision
    float rayDirX = cosf(rayAngle);
    float rayDirY = sinf(rayAngle);
    
    // Use the BSP tree to cast the ray
    CudaVec2 rayOrigin(playerX, playerY);
    CudaVec2 rayDir(rayDirX, rayDirY);
    
    // Special debug rays at fixed positions for testing
    // bool isDebugRay = (x == 0 || x == width/4 || x == width/2 || x == 3*width/4 || x == width-1);
    bool isDebugRay = false; // Disable debug rays
    
    // Cast ray through BSP tree
    CudaWallCollision collision = castRayBSP(*bspTree, rayOrigin, rayDir, maxDistance);
    
    if (collision.collision) {
        // Correct for fisheye effect
        float correctedDistance = collision.distance * cosf(rayAngle - playerAngle);
        
        // Ensure distance is never too small to prevent division by zero
        correctedDistance = fmaxf(correctedDistance, 0.1f);
        
        // Use a dynamic distance factor based on view distance
        float distanceFactor = 600.0f / correctedDistance;
        
        // Fixed wall height that's known to work consistently
        float projectedWallHeight = 1.0f * distanceFactor;
        
        // Calculate wall top and bottom screen positions - center around middle of screen
        float wallMidY = height / 2.0f;
        int wallTop = fmaxf(0, (int)(wallMidY - projectedWallHeight / 2));
        int wallBottom = fminf(height - 1, (int)(wallMidY + projectedWallHeight / 2));
        
        // Ensure wall height is at least 1 pixel
        if (wallTop >= wallBottom) {
            wallTop = fmaxf(0, (int)(wallMidY - 1));
            wallBottom = fminf(height - 1, (int)(wallMidY + 1));
        }
        
        // Calculate lighting based on distance and wall light level
        float intensity = 1.0f - fminf(0.9f, correctedDistance / maxDistance); // Cap at 0.9 to prevent totally black walls
        intensity = fmaxf(0.3f, intensity); // Ensure walls are never too dark
        
        // Draw the wall column
        for (int y = wallTop; y <= wallBottom; y++) {
            // Calculate texture coordinate (0-1 range)
            float wallPercent = (float)(y - wallTop) / fmaxf(1, wallBottom - wallTop);
            
            // Get the wall color
            Color wallColor;
            bool useTexture = false;
            
            // Try to use texture if available and valid
            if (textures != nullptr && collision.textureId >= 0 && collision.textureId < numTextures) {
                CudaRenderData::TextureData texture = textures[collision.textureId];
                
                if (texture.pixels != nullptr && texture.width > 0 && texture.height > 0) {
                    useTexture = true;
                    
                    // Calculate texture coordinates
                    float texU = collision.texCoordU;
                    float texV = wallPercent;
                    
                    // Ensure texture coordinates are in [0,1] range
                    texU = texU - floorf(texU);
                    texV = texV - floorf(texV);
                    
                    // Get texture pixel indices
                    int texX = (int)(texU * texture.width);
                    int texY = (int)(texV * texture.height);
                    
                    // Clamp to texture dimensions
                    texX = fmaxf(0, fminf(texture.width - 1, texX));
                    texY = fmaxf(0, fminf(texture.height - 1, texY));
                    
                    // Get the texel color
                    int texIndex = texY * texture.width + texX;
                    Color texColor = texture.pixels[texIndex];
                    
                    // Apply lighting
                    wallColor.r = (uint8_t)(texColor.r * intensity);
                    wallColor.g = (uint8_t)(texColor.g * intensity);
                    wallColor.b = (uint8_t)(texColor.b * intensity);
                    wallColor.a = 255; // Fully opaque
                    
                    // Add portal effect if needed
                    if (collision.isPortal) {
                        // Give portals a slight blue tint
                        wallColor.b = fminf(255, (int)(wallColor.b * 1.2f));
                    }
                }
            }
            
            // Fallback to solid color if texture not available or invalid
            if (!useTexture) {
                // More visible wall colors for debugging
                if (collision.isPortal) {
                    // Portal wall fallback - bright blue
                    wallColor = Color(
                        (uint8_t)(60 * intensity), 
                        (uint8_t)(60 * intensity), 
                        (uint8_t)(220 * intensity),
                        255
                    );
                } else {
                    // Regular wall fallback - use distinct colors based on texture ID
                    // Use a vibrant color scheme for better visibility
                    int baseHue = (collision.textureId % 6) * 60; // 6 distinct colors
                    
                    if (baseHue < 60) {
                        // Red to Yellow
                        wallColor = Color(
                            255,
                            (uint8_t)((baseHue/60.0f) * 255 * intensity),
                            0,
                            255
                        );
                    } else if (baseHue < 120) {
                        // Yellow to Green
                        wallColor = Color(
                            (uint8_t)((2.0f - baseHue/60.0f) * 255 * intensity),
                            255,
                            0,
                            255
                        );
                    } else if (baseHue < 180) {
                        // Green to Cyan
                        wallColor = Color(
                            0,
                            255,
                            (uint8_t)((baseHue/60.0f - 2.0f) * 255 * intensity),
                            255
                        );
                    } else if (baseHue < 240) {
                        // Cyan to Blue
                        wallColor = Color(
                            0,
                            (uint8_t)((4.0f - baseHue/60.0f) * 255 * intensity),
                            255,
                            255
                        );
                    } else if (baseHue < 300) {
                        // Blue to Magenta
                        wallColor = Color(
                            (uint8_t)((baseHue/60.0f - 4.0f) * 255 * intensity),
                            0,
                            255,
                            255
                        );
                    } else {
                        // Magenta to Red
                        wallColor = Color(
                            255,
                            0,
                            (uint8_t)((6.0f - baseHue/60.0f) * 255 * intensity),
                            255
                        );
                    }
                }
            }
            
            // Special debug ray visualization
            if (isDebugRay) {
                // Only mark the middle of the wall for debug rays
                int wallHeight = wallBottom - wallTop;
                if (y >= wallTop + wallHeight/3 && y <= wallBottom - wallHeight/3) {
                    // Choose color based on ray position
                    if (x == 0) wallColor = Color(255, 0, 0, 255); // Red
                    else if (x == width/4) wallColor = Color(255, 255, 0, 255); // Yellow
                    else if (x == width/2) wallColor = Color(0, 255, 0, 255); // Green
                    else if (x == 3*width/4) wallColor = Color(0, 255, 255, 255); // Cyan
                    else wallColor = Color(0, 0, 255, 255); // Blue
                }
            }
            
            // Set pixel with improved depth testing (larger tolerance for distant walls)
            int idx = y * width + x;
            float depth = correctedDistance / maxDistance;
            
            // More tolerance for distant walls to prevent z-fighting
            float tolerance = 0.001f + (depth * 0.01f);
            
            // Use proper z-buffer check instead of bypassing it
            // This ensures correct depth ordering between walls, platforms, and floors
            if (depth <= zBuffer[idx] + tolerance) {
                frameBuffer[idx] = wallColor;
                zBuffer[idx] = depth;
            }
        }
    } else if (isDebugRay) {
        // Draw a thin line for debug rays that didn't hit anything
        Color debugColor(255, 0, 255, 255); // Magenta
        
        // Draw line in the middle of the screen
        int midY = height / 2;
        for (int y = midY - 2; y <= midY + 2; y++) {
            if (y >= 0 && y < height) {
                int idx = y * width + x;
                // Always draw debug rays
                frameBuffer[idx] = debugColor;
                zBuffer[idx] = 0.95f;
            }
        }
    }
}

// Floor rendering kernel (ceiling is handled by skybox)
__global__ void floorRenderKernel(
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
    float ceilingHeight,
    int floorTextureId,
    int lightLevel,
    CudaRenderData::TextureData* textures,
    int numTextures)
{
    // Calculate the current pixel coordinates
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    // Early exit if outside screen bounds
    if (x >= width || y >= height) return;
    
    // Skip if pixel is not in the floor section (below horizon)
    int horizon = height / 2;
    
    // Only process floor below horizon (ceiling is now handled by skybox)
    if (y <= horizon) return;
    
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
    // Floor rendering - calculate distance based on screen Y
    float verticalAngle = (y - horizon) / (float)(height - horizon);
    
    // Avoid division by zero and ensure minimum visibility
    verticalAngle = fmaxf(0.005f, verticalAngle);
    
    // Calculate the distance to the point on floor
    float heightDiff = playerHeight - floorHeight;
    
    // Improved distance calculation with better scaling
    float distance = heightDiff / verticalAngle * DISTANCE_MULTIPLIER / height;
    
    // Ensure minimum distance for better visibility
    distance = fmaxf(0.1f, distance);
    
    // If too far, don't render (fog)
    if (distance > maxDistance) return;
    
    // Calculate world position
    float worldX = playerX + rayDirX * distance;
    float worldY = playerY + rayDirY * distance;
    
    // Improved texture coordinates with scaling for better detail
    float texScale = 1.0f; // Adjust this value to change texture scale
    float texU = fmodf(worldX * texScale, 1.0f);
    float texV = fmodf(worldY * texScale, 1.0f);
    
    if (texU < 0) texU += 1.0f;
    if (texV < 0) texV += 1.0f;
    
    // Apply a distance fog effect with improved visibility
    float fogFactor = 1.0f - fminf(0.95f, distance / maxDistance); // Cap at 0.95 to ensure some visibility
    
    // Apply lighting factor (0-1) from current sector with minimum brightness
    float lightFactor = fminf(1.0f, fmaxf(0.3f, lightLevel / 255.0f)); // Increased minimum brightness
    
    // Combined lighting and fog
    float combinedLighting = lightFactor * fogFactor;
    
    Color floorColor;
    
    // Use actual texture if valid ID, otherwise use DOOM-like floor pattern
    if (floorTextureId >= 0 && floorTextureId < numTextures && 
        textures[floorTextureId].pixels != nullptr && 
        textures[floorTextureId].width > 0 && 
        textures[floorTextureId].height > 0) {
        // Get texture info
        CudaRenderData::TextureData texture = textures[floorTextureId];
        
        // Sample the texture
        int texX = (int)(texU * texture.width);
        int texY = (int)(texV * texture.height);
        
        // Ensure texture coordinates are within bounds
        texX = fmaxf(0, fminf(texture.width - 1, texX));
        texY = fmaxf(0, fminf(texture.height - 1, texY));
        
        // Get the texel color
        int texIndex = texY * texture.width + texX;
        floorColor = texture.pixels[texIndex];
    } else {
        // Fallback to DOOM-like floor pattern instead of checkerboard
        
        // Create a small-scale grid for the floor
        // Scale coordinates to create a tighter pattern
        float gridScale = 8.0f;
        float gridX = fmodf(worldX * gridScale, 1.0f);
        float gridY = fmodf(worldY * gridScale, 1.0f);
        
        // Create a subtle edge highlight effect
        bool isEdgeX = gridX < 0.05f || gridX > 0.95f;
        bool isEdgeY = gridY < 0.05f || gridY > 0.95f;
        
        // Add some variation based on position to create a subtle pattern
        float noise = fmodf(sinf(worldX * 37.0f + worldY * 23.9f) * 0.5f + 0.5f, 1.0f);
        noise = noise * 0.15f + 0.85f; // Limit the noise effect to 15%
        
        // Base DOOM-like floor colors - dark grayish brown
        uint8_t baseR = 48;
        uint8_t baseG = 42;
        uint8_t baseB = 35;
        
        // Lighter highlights for edges
        if (isEdgeX || isEdgeY) {
            baseR = 58;
            baseG = 52;
            baseB = 45;
        }
        
        // Apply subtle noise variation
        floorColor.r = (uint8_t)(baseR * noise);
        floorColor.g = (uint8_t)(baseG * noise);
        floorColor.b = (uint8_t)(baseB * noise);
        floorColor.a = 255;
    }
    
    // Apply lighting and fog
    floorColor.r = (uint8_t)(floorColor.r * combinedLighting);
    floorColor.g = (uint8_t)(floorColor.g * combinedLighting);
    floorColor.b = (uint8_t)(floorColor.b * combinedLighting);
    
    // Set the pixel with proper z-buffer check
    int idx = y * width + x;
    float depth = distance / maxDistance;
    
    // Use the same tolerance approach as walls for consistent rendering
    float tolerance = 0.001f + (depth * 0.01f);
    
    // Only draw if this pixel is closer than what's already there (with tolerance)
    if (depth <= zBuffer[idx] + tolerance) {
        frameBuffer[idx] = floorColor;
        zBuffer[idx] = depth;
    }
}

// This kernel will be called once per sprite
__global__ void spriteRenderKernel(Color* frameBuffer, float* zBuffer, 
                                  int width, int height,
                                  float playerX, float playerY, float playerAngle, 
                                  float playerHeight, float fov,
                                  CudaSpriteData sprite,
                                  int textureWidth, int textureHeight, 
                                  Color* texturePixels) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height || !sprite.visible) return;
    
    // Safety check for null texture
    if (texturePixels == nullptr || textureWidth <= 0 || textureHeight <= 0) {
        // Draw a simple placeholder if texture is missing
        if (sprite.distance < zBuffer[y * width + x]) {
            // Draw a simple purple placeholder
            Color purpleColor;
            purpleColor.r = 128;
            purpleColor.g = 0;
            purpleColor.b = 128;
            purpleColor.a = 255;
            frameBuffer[y * width + x] = purpleColor;
        }
        return;
    }
    
    // Calculate angle to sprite relative to player view
    float dx = sprite.x - playerX;
    float dy = sprite.y - playerY;
    float spriteAngle = atan2f(dy, dx);
    
    // Adjust sprite angle relative to player angle
    float angleDiff = spriteAngle - playerAngle;
    if (angleDiff < -M_PI) angleDiff += 2.0f * M_PI;
    if (angleDiff > M_PI) angleDiff -= 2.0f * M_PI;
    
    // Skip if sprite is behind player (outside field of view + margin)
    const float fovMargin = 0.2f; // Additional margin beyond FOV
    if (fabsf(angleDiff) > (fov / 2.0f + fovMargin)) return;
    
    // Calculate sprite position on screen
    int spriteScreenX = static_cast<int>((0.5f + angleDiff / fov) * width);
    
    // Calculate sprite dimensions
    float spriteSize = (height / sprite.distance) * sprite.scale;
    int spriteWidth = static_cast<int>(spriteSize);
    int spriteHeight = static_cast<int>(spriteSize);
    
    // Skip if sprite is too small
    if (spriteWidth <= 0 || spriteHeight <= 0) return;
    
    // Calculate sprite screen position
    int drawStartX = spriteScreenX - spriteWidth / 2;
    int drawEndX = spriteScreenX + spriteWidth / 2;
    
    // Calculate vertical position based on player height
    float heightOffset = (playerHeight - 0.5f) * 100.0f / sprite.distance;
    int drawStartY = height / 2 - spriteHeight / 2 + static_cast<int>(heightOffset);
    int drawEndY = height / 2 + spriteHeight / 2 + static_cast<int>(heightOffset);
    
    // Check if this pixel is within the sprite bounds
    if (x < drawStartX || x >= drawEndX || y < drawStartY || y >= drawEndY) return;
    
    // Calculate texture coordinates using nearest neighbor sampling
    int texX = static_cast<int>((x - drawStartX) * textureWidth / (float)(drawEndX - drawStartX));
    int texY = static_cast<int>((y - drawStartY) * textureHeight / (float)(drawEndY - drawStartY));
    
    // Clamp texture coordinates
    texX = max(0, min(texX, textureWidth - 1));
    texY = max(0, min(texY, textureHeight - 1));
    
    // Get texture color with bounds checking
    Color texColor;
    if (texX >= 0 && texX < textureWidth && texY >= 0 && texY < textureHeight) {
        texColor = texturePixels[texY * textureWidth + texX];
    } else {
        // Fallback color if texture coordinates are out of bounds
        texColor.r = 0;
        texColor.g = 255;
        texColor.b = 0;
        texColor.a = 255;
        return;
    }
    
    // Skip if pixel is transparent (alpha < 128)
    if (texColor.a < 128) return;
    
    // Apply fog effect based on distance
    float fogFactor = min(1.0f, max(0.0f, sprite.distance / 20.0f));
    texColor.r = static_cast<uint8_t>(texColor.r * (1.0f - fogFactor));
    texColor.g = static_cast<uint8_t>(texColor.g * (1.0f - fogFactor));
    texColor.b = static_cast<uint8_t>(texColor.b * (1.0f - fogFactor));
    texColor.a = 255; // Ensure fully opaque after fog
    
    // Depth test
    if (sprite.distance < zBuffer[y * width + x]) {
        frameBuffer[y * width + x] = texColor;
        // Don't update z-buffer for partially transparent sprites
        // zBuffer[y * width + x] = sprite.distance;
    }
}

// Kernel to clear z-buffer to a specific value
__global__ void clearZBufferKernel(float* zBuffer, int width, int height, float clearValue) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x < width && y < height) {
        zBuffer[y * width + x] = clearValue;
    }
}

// Kernel to clear frame buffer to a specific color
__global__ void clearFrameBufferKernel(Color* frameBuffer, int width, int height, Color clearColor) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x < width && y < height) {
        frameBuffer[y * width + x] = clearColor;
    }
}

// Host code for RendererCuda implementation

RendererCuda::RendererCuda(int width, int height)
    : m_width(width), m_height(height), m_cudaAvailable(false), m_initialized(false),
      m_buffersAllocated(false), m_texturesUploaded(false), m_bspUploaded(false), 
      m_usingTestMap(false), m_cudaData(nullptr)
{
    // Check CUDA availability
    m_deviceInfo = getCudaDeviceInfo();
    m_cudaAvailable = m_deviceInfo.available;
    
    if (m_cudaAvailable) {
        std::cout << "CUDA is available for rendering acceleration" << std::endl;
        printCudaDeviceInfo(m_deviceInfo);
    } else {
        std::cout << "CUDA is not available, using CPU rendering" << std::endl;
    }
    
    // Initialize to empty BSP tree
    memset(&m_deviceBSPTree, 0, sizeof(m_deviceBSPTree));
}

RendererCuda::~RendererCuda() {
    try {
        cleanup();
    } catch (const std::exception& e) {
        std::cerr << "Error during CUDA renderer cleanup in destructor: " << e.what() << std::endl;
    }
}

void RendererCuda::cleanup() {
    try {
        if (m_initialized) {
            freeCudaMemory();
        }
        
        if (m_cudaData) {
            delete m_cudaData;
            m_cudaData = nullptr;
        }
        
        if (m_cudaAvailable) {
            cleanupCuda();
        }
        
        m_initialized = false;
        m_buffersAllocated = false;
        m_texturesUploaded = false;
        m_bspUploaded = false;
    } catch (const std::exception& e) {
        std::cerr << "Error in CUDA cleanup: " << e.what() << std::endl;
    }
}

bool RendererCuda::initialize() {
    try {
        if (!m_cudaAvailable) {
            std::cerr << "CUDA is not available for initialization" << std::endl;
            return false;
        }
        
        // Clean up any existing data
        cleanup();
        
        // Allocate cudaData structure
        m_cudaData = new CudaRenderData();
        if (!m_cudaData) {
            std::cerr << "Failed to allocate CUDA data structure" << std::endl;
            return false;
        }
        
        m_cudaData->width = m_width;
        m_cudaData->height = m_height;
        m_cudaData->d_frameBuffer = nullptr;
        m_cudaData->d_zBuffer = nullptr;
        m_cudaData->d_textures = nullptr;
        m_cudaData->numTextures = 0;
        m_cudaData->d_bspTree = nullptr;
        
        m_initialized = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing CUDA renderer: " << e.what() << std::endl;
        cleanup();
        return false;
    }
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
    
    try {
        // Free device memory for frame buffer
        if (m_cudaData->d_frameBuffer) {
            cudaError_t err = cudaFree(m_cudaData->d_frameBuffer);
            if (err != cudaSuccess) {
                std::cerr << "Error freeing frame buffer: " << cudaGetErrorString(err) << std::endl;
            }
            m_cudaData->d_frameBuffer = nullptr;
        }
        
        // Free device memory for Z-buffer
        if (m_cudaData->d_zBuffer) {
            cudaError_t err = cudaFree(m_cudaData->d_zBuffer);
            if (err != cudaSuccess) {
                std::cerr << "Error freeing Z-buffer: " << cudaGetErrorString(err) << std::endl;
            }
            m_cudaData->d_zBuffer = nullptr;
        }
        
        m_buffersAllocated = false;
    } catch (const std::exception& e) {
        std::cerr << "Error in freeBuffers: " << e.what() << std::endl;
        m_buffersAllocated = false;
    }
}

void RendererCuda::clearBuffers() {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    if (!m_buffersAllocated || m_cudaData->d_frameBuffer == nullptr || m_cudaData->d_zBuffer == nullptr) {
        std::cerr << "Cannot clear buffers: not allocated" << std::endl;
        return;
    }
    
    try {
        // Use kernels to properly clear buffers
        dim3 blockSize(16, 16);
        dim3 gridSize(
            (m_width + blockSize.x - 1) / blockSize.x,
            (m_height + blockSize.y - 1) / blockSize.y
        );
        
        // Clear frame buffer to black
        clearFrameBufferKernel<<<gridSize, blockSize>>>(
            m_cudaData->d_frameBuffer,
            m_width,
            m_height,
            Color(0, 0, 0, 255)  // Black, fully opaque
        );
        
        // Clear z-buffer to maximum depth (FLT_MAX for better precision)
        clearZBufferKernel<<<gridSize, blockSize>>>(
            m_cudaData->d_zBuffer,
            m_width,
            m_height,
            FLT_MAX  // Maximum depth for better precision
        );
        
        // Check for errors
        CUDA_CHECK(cudaGetLastError());
    } catch (const std::exception& e) {
        std::cerr << "Error clearing buffers: " << e.what() << std::endl;
    }
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
    
    try {
        // Free buffer memory
        freeBuffers();
        
        // Free texture data
        if (m_cudaData->d_textures) {
            // Free each texture's pixel data
            for (int i = 0; i < m_cudaData->numTextures; ++i) {
                if (m_cudaData->d_textures[i].pixels) {
                    CUDA_CHECK(cudaFree(m_cudaData->d_textures[i].pixels));
                    m_cudaData->d_textures[i].pixels = nullptr;
                }
            }
            
            // Free the texture array
            CUDA_CHECK(cudaFree(m_cudaData->d_textures));
            m_cudaData->d_textures = nullptr;
            m_cudaData->numTextures = 0;
            m_texturesUploaded = false;
        }
        
        // Free BSP data
        freeBSPData();
    } catch (const std::exception& e) {
        std::cerr << "Error in freeCudaMemory: " << e.what() << std::endl;
    }
}

void RendererCuda::renderFrame(const BSPTree& bsp, const ViewPosition& view,
                            const std::vector<Sprite>& sprites, float deltaTime) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    // Make sure buffers are allocated
    if (!m_buffersAllocated) {
        allocateBuffers();
    }
    
    try {
        // Store current view position for ray casting
        m_cudaData->currentView = view;
        
        // Ensure BSP data is uploaded - Use a local copy of the BSP tree to prevent race conditions
        bool needsUpload = !m_bspUploaded;
        
        // Check if textures need to be uploaded or re-uploaded
        bool needsTextureUpload = !m_texturesUploaded;
        
        // Check for any invalid texture references in sprites
        for (const auto& sprite : sprites) {
            const SpriteFrame& frame = sprite.getCurrentFrame();
            int textureId = frame.textureId;
            if (textureId >= m_cudaData->numTextures) {
                // We found a sprite referring to a texture that isn't uploaded
                needsTextureUpload = true;
                std::cout << "Detected sprite using texture ID " << textureId 
                          << " which exceeds current texture count " << m_cudaData->numTextures 
                          << ". Re-uploading textures." << std::endl;
                break;
            }
        }
        
        if (needsUpload) {
            serializeBSPForCuda(bsp);
            if (!m_bspUploaded) {
                std::cerr << "Failed to upload BSP data, skipping rendering" << std::endl;
                return;
            }
        }
        
        // Clear buffers for new frame
        clearBuffers();
        
        // 1. First render the skybox as the background (includes the ceiling)
        renderSkyboxCuda(view, deltaTime, m_skybox);
        
        // 2. Then render BSP walls which will properly occlude parts of the skybox
        renderBSPCuda(bsp, view, m_skybox.maxViewDistance);
        
        // 3. Render floor (ceiling is now handled by skybox)
        renderFloorCuda(bsp, view);
        
        // 4. Render sprites
        if (!sprites.empty()) {
            renderSpritesCuda(bsp, view, sprites);
        }
        
        // 5. Apply directional lighting from the sun with shadows
        if (hasCustomParameter("enableHellSun") && getCustomParameter("enableHellSun") > 0.5f) {
            // Cast shadow rays from the sun to determine shadows
            int numRays = static_cast<int>(getCustomParameter("numLightRays"));
            if (numRays > 0) {
                // Get sun angle and calculate screen position
                float sunAngle = m_skybox.sunAngle;
                float sunHeight = m_skybox.sunHeight;
                
                // Calculate sun position on screen
                float adjustedSunAngle = sunAngle - view.angle;
                while (adjustedSunAngle < -PI) adjustedSunAngle += 2.0f * PI;
                while (adjustedSunAngle > PI) adjustedSunAngle -= 2.0f * PI;
                
                // Convert sun angle to screen x-coordinate
                float fovRadians = view.fov * (PI / 180.0f);
                float normalizedAngle = adjustedSunAngle / (fovRadians/2); // -1 to 1
                float screenX = m_width * (0.5f + 0.5f * normalizedAngle);
                
                // Convert sun height to screen y-coordinate
                float horizonY = m_height / 2.0f;
                float sunHeightOffset = sunHeight * m_height * 0.5f;
                float screenY = horizonY - sunHeightOffset;
                
                // Prepare custom parameters and shadow hit buffer
                float customParams[10] = {0}; // Increased to 10 for new parameters
                customParams[0] = getCustomParameter("enableHellSun");
                customParams[1] = static_cast<float>(numRays);
                customParams[2] = getCustomParameter("rayLength");
                customParams[3] = getCustomParameter("rayWidth");
                customParams[4] = getCustomParameter("rayIntensity");
                customParams[5] = getCustomParameter("enableShadowCasting");
                customParams[6] = getCustomParameter("shadowIntensity");
                customParams[7] = getCustomParameter("shadowSoftness");
                customParams[8] = getCustomParameter("mapLightingIntensity");
                customParams[9] = getCustomParameter("lightAttenuation");
                
                // Allocate memory for custom parameters and shadow hit buffer
                float* d_customParams = nullptr;
                int* d_wallHitBuffer = nullptr;
                
                // Allocate and copy custom parameters to device
                cudaMalloc((void**)&d_customParams, 10 * sizeof(float)); // Increased to 10
                cudaMemcpy(d_customParams, customParams, 10 * sizeof(float), cudaMemcpyHostToDevice);
                
                // Allocate memory for shadow hit buffer
                cudaMalloc((void**)&d_wallHitBuffer, numRays * sizeof(int));
                
                // Launch the shadow ray casting kernel to determine shadow areas
                int threadsPerBlock = 128;
                int blocksPerGrid = (numRays + threadsPerBlock - 1) / threadsPerBlock;
                
                shadowRayCastKernel<<<blocksPerGrid, threadsPerBlock>>>(
                    d_wallHitBuffer,
                    view.position.x, view.position.y,
                    sunAngle, sunHeight,
                    m_deviceBSPTree,
                    customParams[2] * 5.0f, // Convert ray length to world units
                    numRays
                );
                
                // Launch the directional lighting kernel
                dim3 blockSize(16, 16);
                dim3 gridSize((m_width + blockSize.x - 1) / blockSize.x, 
                             (m_height + blockSize.y - 1) / blockSize.y);
                
                applyDirectionalLightKernel<<<gridSize, blockSize>>>(
                    m_cudaData->d_frameBuffer,
                    m_cudaData->d_zBuffer,
                    m_width,
                    m_height,
                    sunAngle,
                    sunHeight,
                    screenX,
                    screenY,
                    view.position.x,
                    view.position.y,
                    view.angle,
                    m_skybox.sunColor,
                    customParams[4], // rayIntensity
                    d_customParams,
                    d_wallHitBuffer,
                    numRays,
                    m_skybox.maxViewDistance
                );
                
                // Free device memory
                if (d_customParams) cudaFree(d_customParams);
                if (d_wallHitBuffer) cudaFree(d_wallHitBuffer);
            }
        }
        
        // Synchronize to ensure all kernel calls complete
        cudaDeviceSynchronize();
        
    } catch (const std::exception& e) {
        std::cerr << "Error in renderFrame: " << e.what() << std::endl;
    }
}

void RendererCuda::renderSkyboxCuda(const ViewPosition& view, float deltaTime, const Skybox& skybox) {
    if (!m_initialized || !m_buffersAllocated) return;
    
    // Store current view for use in ray casting
    m_cudaData->currentView = view;
    
    // Update time of day in skybox if dynamic
    if (skybox.dynamicSky) {
        const_cast<Skybox&>(skybox).update(deltaTime);
    }
    
    // Calculate horizon line
    float horizonY = m_height / 2.0f;
    
    // Determine performance mode
    bool performanceMode = skybox.maxViewDistance < 20.0f;
    
    // Launch the skybox gradient kernel
    dim3 blockSize(16, 16);
    dim3 gridSize((m_width + blockSize.x - 1) / blockSize.x, 
                 (m_height + blockSize.y - 1) / blockSize.y);
    
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
    
    // Draw the sun if dynamic sky is enabled
    if (skybox.dynamicSky) {
        // Calculate sun position on screen
        float sunAngle = skybox.sunAngle - view.angle;
        while (sunAngle < -PI) sunAngle += 2.0f * PI;
        while (sunAngle > PI) sunAngle -= 2.0f * PI;
        
        // Only draw sun if it's in the viewport
        // FOV is typically in degrees, convert to radians
        float fovRadians = view.fov * (PI / 180.0f);
        
        if (sunAngle >= -fovRadians/2 && sunAngle <= fovRadians/2) {
            // Convert sun angle to screen x-coordinate
            float normalizedAngle = sunAngle / (fovRadians/2); // -1 to 1
            float screenX = m_width * (0.5f + 0.5f * normalizedAngle);
            
            // Convert sun height to screen y-coordinate
            float sunHeightOffset = skybox.sunHeight * m_height * 0.5f;
            float screenY = horizonY - sunHeightOffset;
            
            // Calculate sun intensity based on time of day
            float sunIntensity = 1.0f;
            
            // Draw the sun with our custom parameters
            drawSunCuda(screenX, screenY, skybox.sunSize, skybox.sunColor, sunIntensity, skybox);
        }
    }
}

void RendererCuda::drawSunCuda(float screenX, float screenY, float sizeDegrees, const Color& color, 
                             float intensity, const Skybox& skybox) {
    if (!m_initialized || !m_buffersAllocated) return;
    
    // Convert sun size from degrees to pixels (screen space conversion)
    float radius = m_height * (sizeDegrees / 180.0f);
    
    // Create a buffer for custom parameters
    float customParams[8] = {0};
    
    // Transfer custom parameters if they exist
    customParams[0] = getCustomParameter("enableHellSun");
    customParams[1] = getCustomParameter("numLightRays");
    customParams[2] = getCustomParameter("rayLength");
    customParams[3] = getCustomParameter("rayWidth");
    customParams[4] = getCustomParameter("rayIntensity");
    customParams[5] = getCustomParameter("enableShadowCasting");
    customParams[6] = getCustomParameter("shadowIntensity");
    customParams[7] = getCustomParameter("shadowSoftness");
    
    // Device memory for custom parameters
    float* d_customParams = nullptr;
    int* d_wallHitBuffer = nullptr;
    
    // Allocate and copy custom parameters to device
    cudaMalloc((void**)&d_customParams, 8 * sizeof(float));
    cudaMemcpy(d_customParams, customParams, 8 * sizeof(float), cudaMemcpyHostToDevice);
    
    // If shadow casting is enabled, use our shadow ray kernel to calculate shadows
    if (customParams[5] > 0.5f && customParams[1] > 0) {
        int numRays = static_cast<int>(customParams[1]);
        
        // Allocate device memory for the wall hit buffer
        cudaMalloc((void**)&d_wallHitBuffer, numRays * sizeof(int));
        
        // Use 1D thread blocks for ray casting (one thread per ray)
        int threadsPerBlock = 128; // Typical value for CUDA
        int blocksPerGrid = (numRays + threadsPerBlock - 1) / threadsPerBlock;
        
        // Get player position and viewing angle from the current view
        ViewPosition& view = m_cudaData->currentView;
        float playerX = view.position.x;
        float playerY = view.position.y;
        
        // Launch the shadow ray casting kernel
        shadowRayCastKernel<<<blocksPerGrid, threadsPerBlock>>>(
            d_wallHitBuffer,
            playerX, playerY,
            skybox.sunAngle, skybox.sunHeight,
            m_deviceBSPTree,
            customParams[2] * 5.0f, // Convert ray length to world units
            numRays
        );
        
        // Check for errors after shadow ray casting
        cudaError_t rayError = cudaGetLastError();
        if (rayError != cudaSuccess) {
            std::cerr << "Error in shadow ray casting: " << cudaGetErrorString(rayError) << std::endl;
        }
    }
    
    // Calculate thread blocks and grid
    bool performanceMode = skybox.maxViewDistance < 20.0f;
    dim3 blockSize(16, 16);
    dim3 gridSize((m_width + blockSize.x - 1) / blockSize.x, 
                 (m_height + blockSize.y - 1) / blockSize.y);
    
    // Launch the sun kernel with custom parameters
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
        performanceMode,
        d_customParams,
        d_wallHitBuffer
    );
    
    // Free device memory
    if (d_customParams) cudaFree(d_customParams);
    if (d_wallHitBuffer) cudaFree(d_wallHitBuffer);
    
    // Check for errors
    cudaError_t error = cudaGetLastError();
    if (error != cudaSuccess) {
        std::cerr << "Error in drawSunCuda: " << cudaGetErrorString(error) << std::endl;
    }
}

// Update renderBSPCuda to use the serialized BSP tree
void RendererCuda::renderBSPCuda(const BSPTree& bsp, const ViewPosition& view, float maxViewDistance) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    // Get player information
    float playerX = view.position.x;
    float playerY = view.position.y;
    float playerAngle = view.angle;
    float playerHeight = view.height;
    float fov = view.fov;
    
    // Create a static mutex to prevent race conditions when accessing the BSP tree
    static std::mutex bspAccessMutex;
    
    // Try to acquire a lock to check if we need to update BSP data
    {
        std::unique_lock<std::mutex> lock(bspAccessMutex, std::try_to_lock);
        if (lock.owns_lock() && !m_bspUploaded) {
            try {
                serializeBSPForCuda(bsp);
            } catch (const std::exception& e) {
                std::cerr << "Error serializing BSP for rendering: " << e.what() << std::endl;
                // Continue with existing data if available
            }
        }
    }
    
    // Make sure buffers are allocated
    if (!m_buffersAllocated) {
        allocateBuffers();
    }
    
    // Ensure BSP data was successfully uploaded
    if (!m_bspUploaded || !m_cudaData->d_bspTree) {
        std::cerr << "Error: BSP data not available for CUDA rendering" << std::endl;
        return;
    }
    
    // Verify texture status
    if (!m_texturesUploaded || !m_cudaData->d_textures || m_cudaData->numTextures <= 0) {
        std::cerr << "Warning: No textures available for CUDA BSP rendering" << std::endl;
        // Force re-upload on next frame
        m_texturesUploaded = false;
    }
    
    // Check for specific texture IDs required by the BSP - scan sectors for needed textures
    std::set<int> requiredTextureIds;
    
    // Copy sectors and walls data from device to host for checking texture IDs
    if (m_bspUploaded) {
        // We can't directly access device memory, so we need to create host copies
        CudaSector* hostSectors = nullptr;
        CudaWall* hostWalls = nullptr;
        
        try {
            // Check if we have sectors and walls to copy
            if (m_deviceBSPTree.sectorCount > 0 && m_deviceBSPTree.sectors != nullptr) {
                // Allocate host memory for sectors
                hostSectors = new CudaSector[m_deviceBSPTree.sectorCount];
                
                // Copy sectors from device to host
                cudaError_t err = cudaMemcpy(hostSectors, m_deviceBSPTree.sectors, 
                                          m_deviceBSPTree.sectorCount * sizeof(CudaSector), 
                                          cudaMemcpyDeviceToHost);
                if (err != cudaSuccess) {
                    std::cerr << "Failed to copy sectors for texture check: " 
                              << cudaGetErrorString(err) << std::endl;
                } else {
                    // Collect texture IDs from sectors
                    for (int i = 0; i < m_deviceBSPTree.sectorCount; i++) {
                        requiredTextureIds.insert(hostSectors[i].floorTextureId);
                        requiredTextureIds.insert(hostSectors[i].ceilingTextureId);
                    }
                }
            }
            
            // Check if we have walls to copy
            if (m_deviceBSPTree.wallCount > 0 && m_deviceBSPTree.walls != nullptr) {
                // Allocate host memory for walls
                hostWalls = new CudaWall[m_deviceBSPTree.wallCount];
                
                // Copy walls from device to host
                cudaError_t err = cudaMemcpy(hostWalls, m_deviceBSPTree.walls, 
                                          m_deviceBSPTree.wallCount * sizeof(CudaWall), 
                                          cudaMemcpyDeviceToHost);
                if (err != cudaSuccess) {
                    std::cerr << "Failed to copy walls for texture check: " 
                              << cudaGetErrorString(err) << std::endl;
                } else {
                    // Collect texture IDs from walls
                    for (int i = 0; i < m_deviceBSPTree.wallCount; i++) {
                        requiredTextureIds.insert(hostWalls[i].textureId);
                    }
                }
            }
            
            // Check if all required textures are available
            bool missingTextures = false;
            for (int texId : requiredTextureIds) {
                if (texId >= 0 && texId >= m_cudaData->numTextures) { // -1 is valid as "no texture"
                    std::cerr << "Required texture ID " << texId << " is missing from CUDA renderer" << std::endl;
                    missingTextures = true;
                }
            }
            
            if (missingTextures) {
                // Flag that we need to re-upload textures
                m_texturesUploaded = false;
                std::cerr << "Missing textures required by BSP - will request re-upload" << std::endl;
            }
            
            // Clean up host memory
            if (hostSectors) {
                delete[] hostSectors;
            }
            if (hostWalls) {
                delete[] hostWalls;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error checking required textures: " << e.what() << std::endl;
            
            // Clean up if exception occurred
            if (hostSectors) {
                delete[] hostSectors;
            }
            if (hostWalls) {
                delete[] hostWalls;
            }
        }
    }
    
    // Determine thread block and grid sizes
    dim3 blockSize(16, 1);  // Use 16 threads per block for simplicity
    dim3 gridSize((m_width + blockSize.x - 1) / blockSize.x);
    
    // Launch kernel for rendering
    try {
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
            m_cudaData->d_bspTree,
            m_cudaData->d_textures,
            m_cudaData->numTextures
        );
        
        // Check for errors
        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess) {
            std::cerr << "CUDA error in BSP rendering: " << cudaGetErrorString(err) << std::endl;
        } else {
            // Sync after BSP rendering to catch any delayed errors
            err = cudaDeviceSynchronize();
            if (err != cudaSuccess) {
                std::cerr << "CUDA sync error after BSP rendering: " << cudaGetErrorString(err) << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in BSP rendering: " << e.what() << std::endl;
    }
}

void RendererCuda::renderFloorCuda(const BSPTree& bsp, const ViewPosition& view) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    try {
        // Check if device buffers are allocated
        if (!m_buffersAllocated || !m_cudaData->d_frameBuffer || !m_cudaData->d_zBuffer) {
            std::cerr << "Error: CUDA buffers not allocated for floor rendering" << std::endl;
            return;
        }
        
        // Get player information
        float playerX = view.position.x;
        float playerY = view.position.y;
        float playerAngle = view.angle;
        float playerHeight = view.height;
        float fov = view.fov;
        
        // Get current sector information (floor and ceiling heights)
        float floorHeight = 0.0f;
        float ceilingHeight = 2.0f;  // Default ceiling height
        int floorTextureId = 0;      // Default floor texture ID
        int lightLevel = 255;        // Default light level
        
        // Create a static mutex to prevent race conditions when accessing the BSP tree
        static std::mutex bspAccessMutex;
        
        // Local copy to prevent race conditions if BSP is being updated
        std::vector<Sector> sectors;
        int playerSectorId = -1;
        
        // Try to get BSP data with mutex protection
        {
            std::unique_lock<std::mutex> lock(bspAccessMutex, std::try_to_lock);
            
            if (lock.owns_lock()) {
                try {
                    // Only access the BSP if we can lock the mutex
                    sectors = bsp.getSectors();
                    playerSectorId = bsp.findSector(view.position);
                } catch (const std::exception& e) {
                    std::cerr << "Error accessing BSP data for floor rendering: " << e.what() << std::endl;
                    // Continue with default values
                }
            } else {
                // Couldn't lock the mutex, BSP might be being updated
                std::cerr << "Warning: BSP is locked, using default floor values" << std::endl;
            }
        }
        
        // Use sector data if valid
        if (playerSectorId >= 0 && playerSectorId < static_cast<int>(sectors.size())) {
            const Sector& playerSector = sectors[playerSectorId];
            floorHeight = playerSector.floorHeight;
            ceilingHeight = playerSector.ceilingHeight;
            floorTextureId = playerSector.floorTextureId;
            lightLevel = playerSector.lightLevel;
        }
        
        // Use the maxViewDistance from skybox
        float maxViewDistance = m_skybox.maxViewDistance;
        
        // Check if textures are available
        if (!m_texturesUploaded || !m_cudaData->d_textures || m_cudaData->numTextures <= 0) {
            std::cerr << "Warning: No textures available for CUDA floor rendering" << std::endl;
            // We'll continue and the kernel will handle this case with a fallback
        }
        
        // Determine thread block and grid sizes - use 2D grid for floor
        dim3 blockSize(16, 16);  // 16x16 threads per block
        dim3 gridSize(
            (m_width + blockSize.x - 1) / blockSize.x,
            (m_height + blockSize.y - 1) / blockSize.y
        );
        
        // Launch the floor rendering kernel
        floorRenderKernel<<<gridSize, blockSize>>>(
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
            ceilingHeight,
            floorTextureId,
            lightLevel,
            m_cudaData->d_textures,
            m_cudaData->numTextures
        );
        
        // Check for errors
        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess) {
            std::cerr << "CUDA error in floor rendering: " << cudaGetErrorString(err) << std::endl;
        } else {
            // Sync after kernel execution to catch any delayed errors
            err = cudaDeviceSynchronize();
            if (err != cudaSuccess) {
                std::cerr << "CUDA sync error after floor rendering: " << cudaGetErrorString(err) << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in renderFloorCuda: " << e.what() << std::endl;
    }
}

void RendererCuda::renderSpritesCuda(const BSPTree& bsp, const ViewPosition& view, 
                                   const std::vector<Sprite>& sprites) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData || sprites.empty()) return;
    
    // Skip if textures aren't available
    if (!m_texturesUploaded || !m_cudaData->d_textures || m_cudaData->numTextures <= 0) {
        std::cerr << "Warning: No textures available for CUDA sprite rendering" << std::endl;
        // Flag that textures need to be re-uploaded
        m_texturesUploaded = false;
        return;
    }
    
    // Get player information
    float playerX = view.position.x;
    float playerY = view.position.y;
    float playerAngle = view.angle;
    float playerHeight = view.height;
    float fov = view.fov;
    
    try {
        // Collect required texture IDs for sprites first
        std::set<int> requiredTextureIds;
        for (const auto& sprite : sprites) {
            if (sprite.visible) {
                const SpriteFrame& frame = sprite.getCurrentFrame();
                requiredTextureIds.insert(frame.textureId);
            }
        }
        
        // Check if all required textures are available
        bool missingTextures = false;
        for (int texId : requiredTextureIds) {
            if (texId >= 0 && texId >= m_cudaData->numTextures) {
                std::cerr << "Sprite requires texture ID " << texId 
                          << " which is missing (only have " << m_cudaData->numTextures << ")" << std::endl;
                missingTextures = true;
            }
        }
        
        if (missingTextures) {
            // Flag that textures need to be re-uploaded
            m_texturesUploaded = false;
            std::cerr << "Missing textures required by sprites - will request re-upload" << std::endl;
            return; // Skip rendering this frame
        }
        
        // Sort sprites by distance (farthest to nearest)
        std::vector<std::pair<float, size_t>> sortedIndices;
        for (size_t i = 0; i < sprites.size(); ++i) {
            const Sprite& sprite = sprites[i];
            
            // Skip invalid sprites
            if (!sprite.visible) continue;
            
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
        
        // Track sprite rendering status for debugging
        int renderedSprites = 0;
        int skippedSprites = 0;
        
        // Iterate through sorted sprites
        for (size_t i = 0; i < sortedIndices.size(); ++i) {
            float distance = sortedIndices[i].first;
            size_t index = sortedIndices[i].second;
            const Sprite& sprite = sprites[index];
            
            // Skip if too far or too close
            if (distance > 30.0f || distance < 0.1f) {
                skippedSprites++;
                continue;
            }
            
            // Get the current frame
            const SpriteFrame& frame = sprite.getCurrentFrame();
            int textureId = frame.textureId;
            
            // Check if the texture ID is valid
            if (textureId < 0 || textureId >= m_cudaData->numTextures) {
                // Skip invalid texture ID
                std::cerr << "Sprite texture ID " << textureId << " is out of range (0-" 
                          << (m_cudaData->numTextures - 1) << ")" << std::endl;
                skippedSprites++;
                continue;
            }
            
            // Get texture information
            CudaRenderData::TextureData textureData;
            cudaError_t err = cudaMemcpy(&textureData, 
                                    &(m_cudaData->d_textures[textureId]), 
                                    sizeof(CudaRenderData::TextureData), 
                                    cudaMemcpyDeviceToHost);
            
            if (err != cudaSuccess) {
                std::cerr << "CUDA error fetching texture data: " << cudaGetErrorString(err) << std::endl;
                skippedSprites++;
                continue;
            }
            
            if (textureData.pixels == nullptr || textureData.width <= 0 || textureData.height <= 0) {
                // Skip this sprite if texture is invalid
                std::cerr << "Sprite texture ID " << textureId << " has invalid data (null pixels or zero size)" << std::endl;
                skippedSprites++;
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
            
            // Launch kernel for this sprite using the Color* type
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
            cudaError_t kernelErr = cudaGetLastError();
            if (kernelErr != cudaSuccess) {
                std::cerr << "CUDA error in sprite rendering: " << cudaGetErrorString(kernelErr) << std::endl;
                skippedSprites++;
            } else {
                renderedSprites++;
            }
        }
        
        // Log sprite rendering statistics
        if (sprites.size() > 0) {
            std::cout << "Sprite rendering: " << renderedSprites << " rendered, " 
                      << skippedSprites << " skipped out of " << sprites.size() << " total" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in renderSpritesCuda: " << e.what() << std::endl;
    }
}

// Add a new method to upload textures to the GPU
void RendererCuda::uploadTextures(const std::vector<Texture>& textures) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    // If textures already uploaded, free existing memory first
    if (m_texturesUploaded && m_cudaData->d_textures) {
        try {
            // Free each texture's pixel data
            for (int i = 0; i < m_cudaData->numTextures; ++i) {
                if (m_cudaData->d_textures[i].pixels) {
                    CUDA_CHECK(cudaFree(m_cudaData->d_textures[i].pixels));
                    m_cudaData->d_textures[i].pixels = nullptr;
                }
            }
            
            // Free the texture array
            CUDA_CHECK(cudaFree(m_cudaData->d_textures));
            m_cudaData->d_textures = nullptr;
        } catch (const std::exception& e) {
            std::cerr << "Error freeing texture memory: " << e.what() << std::endl;
            // Clean up any remaining state
            m_cudaData->d_textures = nullptr;
            m_cudaData->numTextures = 0;
            m_texturesUploaded = false;
            return; // Don't attempt to upload new textures if cleanup failed
        }
    }
    
    // Allocate new texture array
    int numTextures = static_cast<int>(textures.size());
    m_cudaData->numTextures = numTextures;
    
    if (numTextures <= 0) {
        // If no textures to upload, just mark as not uploaded
        m_texturesUploaded = false;
        return;
    }
    
    try {
        // Allocate device memory for texture array
        CUDA_CHECK(cudaMalloc(&m_cudaData->d_textures, numTextures * sizeof(CudaRenderData::TextureData)));
        
        // Create a host-side copy of the texture array
        CudaRenderData::TextureData* hostTextures = new CudaRenderData::TextureData[numTextures];
        
        // Upload each texture
        for (int i = 0; i < numTextures; ++i) {
            const Texture& texture = textures[i];
            
            // Skip if texture has no dimensions or pixels
            if (texture.width() <= 0 || texture.height() <= 0 || texture.m_pixels.empty()) {
                hostTextures[i].pixels = nullptr;
                hostTextures[i].width = 0;
                hostTextures[i].height = 0;
                continue;
            }
            
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
    } catch (const std::exception& e) {
        std::cerr << "Error uploading textures to GPU: " << e.what() << std::endl;
        
        // Clean up any allocated memory
        if (m_cudaData->d_textures) {
            cudaFree(m_cudaData->d_textures);
            m_cudaData->d_textures = nullptr;
        }
        
        m_cudaData->numTextures = 0;
        m_texturesUploaded = false;
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

// Serializing the BSP tree for CUDA
void RendererCuda::serializeBSPForCuda(const BSPTree& bsp) {
    
    if (!m_cudaAvailable || !m_initialized) {
        std::cerr << "Can't serialize BSP: CUDA not available or not initialized" << std::endl;
        return;
    }
    
    try {
        // Add a static mutex to synchronize BSP serialization
        static std::mutex serializeMutex;
        std::lock_guard<std::mutex> lock(serializeMutex);
        
        std::cout << "Starting BSP tree serialization for CUDA..." << std::endl;
        
        // First make a local deep copy of the BSP tree data to prevent race conditions
        std::vector<Sector> sectorsCopy;
        try {
            sectorsCopy = bsp.getSectors();
            std::cout << "Retrieved " << sectorsCopy.size() << " sectors for serialization" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error getting sectors for serialization: " << e.what() << std::endl;
            return;
        }
        
        // Free existing BSP data if present
        try {
            freeBSPData();
            std::cout << "Freed existing BSP data" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error freeing existing BSP data: " << e.what() << std::endl;
            // Still continue with the upload attempt
        }
        
        // Continue only if we have sectors
        if (sectorsCopy.empty()) {
            std::cerr << "No sectors to serialize for CUDA" << std::endl;
            return;
        }
        
        // Start by collecting all sectors, walls and prepare for BSP nodes
        std::vector<CudaWall> wallsData;
        std::vector<CudaSector> sectorsData;
        std::vector<CudaBSPNode> nodesData;
        
        // First, process all sectors and build a map of sector walls
        std::map<int, std::vector<size_t>> sectorWallIndices; // Maps sector ID to wall indices in wallsData
        
        std::cout << "Processing sectors and walls..." << std::endl;
        
        // First process all sectors
        for (size_t i = 0; i < sectorsCopy.size(); i++) {
            const Sector& sector = sectorsCopy[i];
            
            // Create CUDA sector
            CudaSector cudaSector;
            cudaSector.floorHeight = sector.floorHeight;
            cudaSector.ceilingHeight = sector.ceilingHeight;
            cudaSector.floorTextureId = sector.floorTextureId;
            cudaSector.ceilingTextureId = sector.ceilingTextureId;
            cudaSector.lightLevel = sector.lightLevel;
            
            // Wall indices will be set later after we process all walls
            cudaSector.wallStartIndex = -1;
            cudaSector.wallCount = 0;
            
            sectorsData.push_back(cudaSector);
        }
        
        std::cout << "Processed " << sectorsData.size() << " sectors" << std::endl;
        
        // Now process all walls from all sectors
        for (size_t i = 0; i < sectorsCopy.size(); i++) {
            const Sector& sector = sectorsCopy[i];
            
            // Store the starting index for this sector's walls
            size_t startIdx = wallsData.size();
            
            // Add all walls from this sector
            for (const Wall& wall : sector.walls) {
                CudaWall cudaWall;
                
                // Convert coordinates
                cudaWall.segment.start.x = wall.segment.start.position.x;
                cudaWall.segment.start.y = wall.segment.start.position.y;
                cudaWall.segment.end.x = wall.segment.end.position.x;
                cudaWall.segment.end.y = wall.segment.end.position.y;
                
                // Add other wall properties
                cudaWall.sectorFront = wall.sectorFront;
                cudaWall.sectorBack = wall.sectorBack;
                cudaWall.textureId = wall.textureId;
                cudaWall.textureOffsetX = wall.textureOffsetX;
                cudaWall.textureOffsetY = wall.textureOffsetY;
                cudaWall.lightLevel = sector.lightLevel; // Use sector light level
                
                // Add to wall data vector
                wallsData.push_back(cudaWall);
                
                // Add wall index to the sector's wall indices
                sectorWallIndices[i].push_back(wallsData.size() - 1);
            }
            
            // Store wall range in the sector data
            if (startIdx < wallsData.size()) {
                sectorsData[i].wallStartIndex = startIdx;
                sectorsData[i].wallCount = wallsData.size() - startIdx;
            }
        }
        
        std::cout << "Processed " << wallsData.size() << " walls across all sectors" << std::endl;
        
        // Skip if we ended up with no walls
        if (wallsData.empty()) {
            std::cerr << "No walls to serialize for CUDA" << std::endl;
            return;
        }
        
        // Now we need to build an actual BSP tree structure for CUDA
        // Since we can't directly access the CPU BSP tree structure,
        // we'll build our own simplified version based on the collected data
        
        std::cout << "Building BSP tree structure for CUDA..." << std::endl;
        
        // Initialize a queue for BFS tree construction (more stable than recursion)
        struct NodeToBuild {
            int nodeIndex; // Index of this node in nodesData
            std::vector<size_t> wallIndices; // Wall indices to process in this node
            int sectorId; // Sector ID if this is a leaf node
        };
        
        std::queue<NodeToBuild> nodesToProcess;
        
        // Start with all walls
        std::vector<size_t> allWallIndices;
        for (size_t i = 0; i < wallsData.size(); i++) {
            allWallIndices.push_back(i);
        }
        
        // Create root node
        nodesData.push_back(CudaBSPNode()); // Reserve index 0 for root
        int rootIndex = 0;
        
        // Queue the root node for processing
        nodesToProcess.push({rootIndex, allWallIndices, -1});
        
        // Process nodes until the queue is empty
        while (!nodesToProcess.empty()) {
            NodeToBuild current = nodesToProcess.front();
            nodesToProcess.pop();
            
            // If no walls or just one wall, create a leaf node
            if (current.wallIndices.empty() || current.wallIndices.size() == 1) {
                CudaBSPNode& node = nodesData[current.nodeIndex];
                node.isLeaf = true;
                
                if (!current.wallIndices.empty()) {
                    int wallIdx = current.wallIndices[0];
                    node.wallStartIndex = wallIdx;
                    node.wallCount = 1;
                    node.sectorId = wallsData[wallIdx].sectorFront;
                } else if (current.sectorId >= 0) {
                    // Use provided sector ID if available
                    node.sectorId = current.sectorId;
                }
                
                continue;
            }
            
            // Choose a wall as partitioner (simplification - just use the first wall)
            size_t partitionerWallIdx = current.wallIndices[0];
            CudaWall& partitionerWall = wallsData[partitionerWallIdx];
            
            // Set the partitioner line in the node
            CudaBSPNode& node = nodesData[current.nodeIndex];
            node.partitioner = partitionerWall.segment;
            node.isLeaf = false;
            
            // Distribute walls to front and back sides
            std::vector<size_t> frontWallIndices;
            std::vector<size_t> backWallIndices;
            
            // Include the partitioner wall in the front side
            frontWallIndices.push_back(partitionerWallIdx);
            
            // Check all other walls
            for (size_t i = 1; i < current.wallIndices.size(); i++) {
                size_t wallIdx = current.wallIndices[i];
                CudaWall& wall = wallsData[wallIdx];
                
                // Simplified wall classification - just check if midpoint is front or back of partitioner
                // This is a simplification - proper implementation would use BSP line classification
                
                // Calculate midpoint of the wall
                float midX = (wall.segment.start.x + wall.segment.end.x) * 0.5f;
                float midY = (wall.segment.start.y + wall.segment.end.y) * 0.5f;
                
                // Calculate vector from partitioner start to midpoint
                float vx = midX - partitionerWall.segment.start.x;
                float vy = midY - partitionerWall.segment.start.y;
                
                // Calculate partitioner direction vector
                float dirX = partitionerWall.segment.end.x - partitionerWall.segment.start.x;
                float dirY = partitionerWall.segment.end.y - partitionerWall.segment.start.y;
                
                // Cross product to determine side (sign of z component in 3D cross product)
                float cross = dirX * vy - dirY * vx;
                
                if (cross >= 0) {
                    frontWallIndices.push_back(wallIdx);
                } else {
                    backWallIndices.push_back(wallIdx);
                }
            }
            
            // If all walls ended up on one side, make this a leaf node
            if (frontWallIndices.size() == current.wallIndices.size() || 
                backWallIndices.size() == 0) {
                
                node.isLeaf = true;
                
                // Find a common sector if possible
                int commonSector = -1;
                for (size_t idx : frontWallIndices) {
                    int sector = wallsData[idx].sectorFront;
                    if (commonSector == -1) {
                        commonSector = sector;
                    } else if (commonSector != sector) {
                        // If sectors differ, just use the first one
                        break;
                    }
                }
                
                node.sectorId = (commonSector != -1) ? commonSector : wallsData[frontWallIndices[0]].sectorFront;
                node.wallStartIndex = frontWallIndices[0]; // Use the first wall
                node.wallCount = frontWallIndices.size();  // Count all walls
                
                continue;
            }
            
            if (backWallIndices.size() == current.wallIndices.size() || 
                frontWallIndices.size() == 0) {
                
                node.isLeaf = true;
                
                // Find a common sector if possible
                int commonSector = -1;
                for (size_t idx : backWallIndices) {
                    int sector = wallsData[idx].sectorFront;
                    if (commonSector == -1) {
                        commonSector = sector;
                    } else if (commonSector != sector) {
                        // If sectors differ, just use the first one
                        break;
                    }
                }
                
                node.sectorId = (commonSector != -1) ? commonSector : wallsData[backWallIndices[0]].sectorFront;
                node.wallStartIndex = backWallIndices[0]; // Use the first wall
                node.wallCount = backWallIndices.size();  // Count all walls
                
                continue;
            }
            
            // Create front child node
            node.frontNodeIndex = nodesData.size();
            nodesData.push_back(CudaBSPNode());
            
            // Create back child node
            node.backNodeIndex = nodesData.size();
            nodesData.push_back(CudaBSPNode());
            
            // Queue child nodes for processing
            nodesToProcess.push({node.frontNodeIndex, frontWallIndices, -1});
            nodesToProcess.push({node.backNodeIndex, backWallIndices, -1});
        }
        
        std::cout << "Built BSP tree with " << nodesData.size() << " nodes" << std::endl;
        
        // Use separate try-catch blocks for each allocation to ensure proper cleanup
        CudaWall* d_walls = nullptr;
        CudaSector* d_sectors = nullptr;
        CudaBSPNode* d_nodes = nullptr;
        CudaBSPTree* d_bspTree = nullptr;
        
        try {
            // Allocate memory for walls
            cudaError_t err = cudaMalloc((void**)&d_walls, wallsData.size() * sizeof(CudaWall));
            if (err != cudaSuccess) {
                throw std::runtime_error(std::string("Failed to allocate memory for walls: ") + 
                                         cudaGetErrorString(err));
            }
            
            // Allocate memory for sectors
            err = cudaMalloc((void**)&d_sectors, sectorsData.size() * sizeof(CudaSector));
            if (err != cudaSuccess) {
                cudaFree(d_walls); // Clean up previously allocated memory
                throw std::runtime_error(std::string("Failed to allocate memory for sectors: ") + 
                                         cudaGetErrorString(err));
            }
            
            // Allocate memory for nodes
            err = cudaMalloc((void**)&d_nodes, nodesData.size() * sizeof(CudaBSPNode));
            if (err != cudaSuccess) {
                cudaFree(d_walls);
                cudaFree(d_sectors);
                throw std::runtime_error(std::string("Failed to allocate memory for nodes: ") + 
                                         cudaGetErrorString(err));
            }
            
            // Allocate memory for BSP tree
            err = cudaMalloc((void**)&d_bspTree, sizeof(CudaBSPTree));
            if (err != cudaSuccess) {
                cudaFree(d_walls);
                cudaFree(d_sectors);
                cudaFree(d_nodes);
                throw std::runtime_error(std::string("Failed to allocate memory for BSP tree: ") + 
                                         cudaGetErrorString(err));
            }
            
            // Copy data to device
            err = cudaMemcpy(d_walls, wallsData.data(), wallsData.size() * sizeof(CudaWall), cudaMemcpyHostToDevice);
            if (err != cudaSuccess) {
                cudaFree(d_walls);
                cudaFree(d_sectors);
                cudaFree(d_nodes);
                cudaFree(d_bspTree);
                throw std::runtime_error(std::string("Failed to copy walls to device: ") + 
                                         cudaGetErrorString(err));
            }
            
            err = cudaMemcpy(d_sectors, sectorsData.data(), sectorsData.size() * sizeof(CudaSector), cudaMemcpyHostToDevice);
            if (err != cudaSuccess) {
                cudaFree(d_walls);
                cudaFree(d_sectors);
                cudaFree(d_nodes);
                cudaFree(d_bspTree);
                throw std::runtime_error(std::string("Failed to copy sectors to device: ") + 
                                         cudaGetErrorString(err));
            }
            
            err = cudaMemcpy(d_nodes, nodesData.data(), nodesData.size() * sizeof(CudaBSPNode), cudaMemcpyHostToDevice);
            if (err != cudaSuccess) {
                cudaFree(d_walls);
                cudaFree(d_sectors);
                cudaFree(d_nodes);
                cudaFree(d_bspTree);
                throw std::runtime_error(std::string("Failed to copy nodes to device: ") + 
                                         cudaGetErrorString(err));
            }
            
            // Create device BSP tree structure
            CudaBSPTree hostBSPTree;
            hostBSPTree.nodes = d_nodes;
            hostBSPTree.nodeCount = nodesData.size();
            hostBSPTree.rootNodeIndex = rootIndex;
            hostBSPTree.walls = d_walls;
            hostBSPTree.wallCount = wallsData.size();
            hostBSPTree.sectors = d_sectors;
            hostBSPTree.sectorCount = sectorsData.size();
            
            // Copy BSP tree structure to device
            err = cudaMemcpy(d_bspTree, &hostBSPTree, sizeof(CudaBSPTree), cudaMemcpyHostToDevice);
            if (err != cudaSuccess) {
                cudaFree(d_walls);
                cudaFree(d_sectors);
                cudaFree(d_nodes);
                cudaFree(d_bspTree);
                throw std::runtime_error(std::string("Failed to copy BSP tree to device: ") + 
                                         cudaGetErrorString(err));
            }
            
            // Ensure we've synced all operations
            err = cudaDeviceSynchronize();
            if (err != cudaSuccess) {
                cudaFree(d_walls);
                cudaFree(d_sectors);
                cudaFree(d_nodes);
                cudaFree(d_bspTree);
                throw std::runtime_error(std::string("Failed to synchronize after BSP upload: ") + 
                                         cudaGetErrorString(err));
            }
            
            // Store device pointers only if everything succeeded
            m_deviceBSPTree.walls = d_walls;
            m_deviceBSPTree.wallCount = wallsData.size();
            m_deviceBSPTree.sectors = d_sectors;
            m_deviceBSPTree.sectorCount = sectorsData.size();
            m_deviceBSPTree.nodes = d_nodes;
            m_deviceBSPTree.nodeCount = nodesData.size();
            m_deviceBSPTree.rootNodeIndex = rootIndex;
            
            m_cudaData->d_bspTree = d_bspTree;
            
            m_bspUploaded = true;
            
            std::cout << "BSP tree serialized successfully for CUDA: " 
                     << wallsData.size() << " walls, " 
                     << sectorsData.size() << " sectors, " 
                     << nodesData.size() << " nodes" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Error serializing BSP tree for CUDA: " << e.what() << std::endl;
            
            // Clean up any resources that might have been allocated
            if (d_walls) cudaFree(d_walls);
            if (d_sectors) cudaFree(d_sectors);
            if (d_nodes) cudaFree(d_nodes);
            if (d_bspTree) cudaFree(d_bspTree);
            
            m_bspUploaded = false;
        }
    } catch (const std::exception& e) {
        std::cerr << "High-level error in serializeBSPForCuda: " << e.what() << std::endl;
        m_bspUploaded = false;
    }
}

// Free BSP data on device
void RendererCuda::freeBSPData() {
    
    if (!m_cudaAvailable) {
        return;
    }
    
    if (!m_bspUploaded) {
        return;
    }
    
    try {
        if (m_deviceBSPTree.walls) {
            cudaError_t err = cudaFree(m_deviceBSPTree.walls);
            if (err != cudaSuccess) {
                std::cerr << "Error freeing BSP walls: " << cudaGetErrorString(err) << std::endl;
            }
            m_deviceBSPTree.walls = nullptr;
        }
        
        if (m_deviceBSPTree.sectors) {
            cudaError_t err = cudaFree(m_deviceBSPTree.sectors);
            if (err != cudaSuccess) {
                std::cerr << "Error freeing BSP sectors: " << cudaGetErrorString(err) << std::endl;
            } 
            m_deviceBSPTree.sectors = nullptr;
        }
        
        if (m_deviceBSPTree.nodes) {
            cudaError_t err = cudaFree(m_deviceBSPTree.nodes);
            if (err != cudaSuccess) {
                std::cerr << "Error freeing BSP nodes: " << cudaGetErrorString(err) << std::endl;
            }
            m_deviceBSPTree.nodes = nullptr;
        }
        
        if (m_cudaData->d_bspTree) {
            cudaError_t err = cudaFree(m_cudaData->d_bspTree);
            if (err != cudaSuccess) {
                std::cerr << "Error freeing BSP tree: " << cudaGetErrorString(err) << std::endl;
            }
            m_cudaData->d_bspTree = nullptr;
        }
        
        m_bspUploaded = false;
    } catch (const std::exception& e) {
        std::cerr << "Error in freeBSPData: " << e.what() << std::endl;
        m_bspUploaded = false;
    }
}

// Enable or disable the test map
bool RendererCuda::useTestMap(bool enable) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return false;
    
    // Debug output for test map usage
    std::cout << "--------- DEBUG TEST MAP TOGGLING ---------" << std::endl;
    std::cout << "Current test map status: " << (m_usingTestMap ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "Requested action: " << (enable ? "ENABLE" : "DISABLE") << std::endl;
    
    // If current state matches requested state, do nothing
    if (m_usingTestMap == enable) {
        std::cout << "No change needed (already in requested state)" << std::endl;
        std::cout << "------------------------------------------" << std::endl;
        return true;
    }
    
    try {
        if (enable) {
            // Switching to test map
            std::cout << "Creating test map..." << std::endl;
            
            // Create test map (replaces regular BSP data)
            m_deviceBSPTree = createSimpleTestMap();
            
            // Check if test map creation was successful
            if (m_deviceBSPTree.nodeCount == 0 || m_deviceBSPTree.wallCount == 0) {
                std::cerr << "Error: Failed to create test map" << std::endl;
                std::cout << "------------------------------------------" << std::endl;
                return false;
            }
            
            // Allocate a pointer for the tree on device
            cudaError_t err = cudaMalloc((void**)&m_cudaData->d_bspTree, sizeof(CudaBSPTree));
            if (err != cudaSuccess) {
                std::cerr << "Failed to allocate memory for test map BSP tree: " << cudaGetErrorString(err) << std::endl;
                freeTestMap(m_deviceBSPTree);
                std::cout << "------------------------------------------" << std::endl;
                return false;
            }
            
            // Copy the structure to the device
            err = cudaMemcpy(m_cudaData->d_bspTree, &m_deviceBSPTree, sizeof(CudaBSPTree), cudaMemcpyHostToDevice);
            if (err != cudaSuccess) {
                std::cerr << "Failed to copy test map BSP tree to device: " << cudaGetErrorString(err) << std::endl;
                cudaFree(m_cudaData->d_bspTree);
                m_cudaData->d_bspTree = nullptr;
                freeTestMap(m_deviceBSPTree);
                std::cout << "------------------------------------------" << std::endl;
                return false;
            }
            
            m_bspUploaded = true;
            m_usingTestMap = true;
            
            std::cout << "Test map creation successful!" << std::endl;
            std::cout << "  Nodes: " << m_deviceBSPTree.nodeCount << std::endl;
            std::cout << "  Walls: " << m_deviceBSPTree.wallCount << std::endl;
            std::cout << "  Sectors: " << m_deviceBSPTree.sectorCount << std::endl;
        } else {
            // Switching back from test map
            std::cout << "Disabling test map..." << std::endl;
            
            // Free device memory for the BSP tree
            if (m_cudaData->d_bspTree) {
                cudaFree(m_cudaData->d_bspTree);
                m_cudaData->d_bspTree = nullptr;
            }
            
            // Free test map resources
            freeTestMap(m_deviceBSPTree);
            
            // Reset BSP data
            memset(&m_deviceBSPTree, 0, sizeof(m_deviceBSPTree));
            m_bspUploaded = false;
            m_usingTestMap = false;
            
            std::cout << "Test map disabled" << std::endl;
        }
        
        std::cout << "------------------------------------------" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error " << (enable ? "enabling" : "disabling") 
                 << " test map: " << e.what() << std::endl;
        std::cout << "------------------------------------------" << std::endl;
        return false;
    }
}

// Enable test map with custom sectors
bool RendererCuda::useTestMapWithSectors(const std::vector<Sector>& sectors) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return false;
    
    // Debug output for test map usage
    std::cout << "--------- DEBUG TEST MAP WITH CUSTOM SECTORS ---------" << std::endl;
    std::cout << "Current test map status: " << (m_usingTestMap ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "Requested action: ENABLE with " << sectors.size() << " sectors" << std::endl;
    
    try {
        // Disable existing test map if it's enabled
        if (m_usingTestMap) {
            // Free device memory for the BSP tree
            if (m_cudaData->d_bspTree) {
                cudaFree(m_cudaData->d_bspTree);
                m_cudaData->d_bspTree = nullptr;
            }
            
            // Free test map resources
            freeTestMap(m_deviceBSPTree);
            
            // Reset BSP data
            memset(&m_deviceBSPTree, 0, sizeof(m_deviceBSPTree));
            m_bspUploaded = false;
            m_usingTestMap = false;
        }
        
        // Create test map from the provided sectors
        std::cout << "Creating test map from " << sectors.size() << " sectors..." << std::endl;
        m_deviceBSPTree = createTestMapFromSectors(sectors);
        
        // Check if test map creation was successful
        if (m_deviceBSPTree.nodeCount == 0 || m_deviceBSPTree.wallCount == 0) {
            std::cerr << "Error: Failed to create test map from sectors" << std::endl;
            std::cout << "------------------------------------------" << std::endl;
            return false;
        }
        
        // Allocate a pointer for the tree on device
        cudaError_t err = cudaMalloc((void**)&m_cudaData->d_bspTree, sizeof(CudaBSPTree));
        if (err != cudaSuccess) {
            std::cerr << "Failed to allocate memory for test map BSP tree: " << cudaGetErrorString(err) << std::endl;
            freeTestMap(m_deviceBSPTree);
            std::cout << "------------------------------------------" << std::endl;
            return false;
        }
        
        // Copy the structure to the device
        err = cudaMemcpy(m_cudaData->d_bspTree, &m_deviceBSPTree, sizeof(CudaBSPTree), cudaMemcpyHostToDevice);
        if (err != cudaSuccess) {
            std::cerr << "Failed to copy test map BSP tree to device: " << cudaGetErrorString(err) << std::endl;
            cudaFree(m_cudaData->d_bspTree);
            m_cudaData->d_bspTree = nullptr;
            freeTestMap(m_deviceBSPTree);
            std::cout << "------------------------------------------" << std::endl;
            return false;
        }
        
        m_bspUploaded = true;
        m_usingTestMap = true;
        
        std::cout << "Test map creation successful!" << std::endl;
        std::cout << "  Nodes: " << m_deviceBSPTree.nodeCount << std::endl;
        std::cout << "  Walls: " << m_deviceBSPTree.wallCount << std::endl;
        std::cout << "  Sectors: " << m_deviceBSPTree.sectorCount << std::endl;
        std::cout << "------------------------------------------" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error enabling test map with custom sectors: " << e.what() << std::endl;
        std::cout << "------------------------------------------" << std::endl;
        return false;
    }
}

// Render a complete frame using the test map
void RendererCuda::renderTestMapFrame(const ViewPosition& view, float deltaTime) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    // Make sure we're using a test map
    if (!m_usingTestMap) {
        std::cerr << "Test map not enabled. Please call useTestMapWithSectors() before rendering." << std::endl;
        return;
    }
    
    // Make sure buffers are allocated
    if (!m_buffersAllocated) {
        allocateBuffers();
    }
    
    // Clear buffers for new frame
    clearBuffers();
    
    // Render all components directly on the GPU
    
    // 1. First render the skybox as the background (includes the ceiling)
    renderSkyboxCuda(view, deltaTime, m_skybox);
    
    // 2. Then render BSP walls which will properly occlude parts of the skybox
    if (m_bspUploaded && m_cudaData->d_bspTree) {
        renderTestMapBSPCuda(view, m_skybox.maxViewDistance);
        
        // 3. Render floor (ceiling is now handled by skybox)
        renderTestMapFloorCuda(view);
    } else {
        std::cerr << "Test map data became invalid during rendering" << std::endl;
    }
    
    // No sprites in test map for simplicity
    
    // Ensure all GPU operations are complete
    cudaError_t err = cudaDeviceSynchronize();
    if (err != cudaSuccess) {
        std::cerr << "CUDA sync error in test map rendering: " << cudaGetErrorString(err) << std::endl;
    }
}

// Render the test map BSP tree
void RendererCuda::renderTestMapBSPCuda(const ViewPosition& view, float maxViewDistance) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    // Get player information
    float playerX = view.position.x;
    float playerY = view.position.y;
    float playerAngle = view.angle;
    float playerHeight = view.height;
    float fov = view.fov;
    
    // Make sure buffers are allocated
    if (!m_buffersAllocated) {
        allocateBuffers();
    }
    
    // Ensure BSP data was successfully created
    if (!m_bspUploaded || !m_cudaData->d_bspTree) {
        std::cerr << "Error: Test map BSP data not available for CUDA rendering" << std::endl;
        return;
    }
    
    // Verify texture status (we need at least 7 textures for the test map)
    if (!m_texturesUploaded || !m_cudaData->d_textures || m_cudaData->numTextures < 7) {
        std::cerr << "Warning: Not enough textures available for CUDA test map rendering" << std::endl;
        std::cerr << "The test map requires at least 7 textures, but only " 
                 << m_cudaData->numTextures << " are available." << std::endl;
        // Continue anyway, the kernel will use fallback colors
    }
    
    // Determine thread block and grid sizes
    dim3 blockSize(16, 1);  // Use 16 threads per block for simplicity
    dim3 gridSize((m_width + blockSize.x - 1) / blockSize.x);
    
    // Launch kernel for rendering
    try {
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
            m_cudaData->d_bspTree,
            m_cudaData->d_textures,
            m_cudaData->numTextures
        );
        
        
        // Check for errors
        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess) {
            std::cerr << "CUDA error in test map BSP rendering: " << cudaGetErrorString(err) << std::endl;
        } else {
            // Sync after BSP rendering to catch any delayed errors
            err = cudaDeviceSynchronize();
            if (err != cudaSuccess) {
                std::cerr << "CUDA sync error after test map BSP rendering: " << cudaGetErrorString(err) << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in test map BSP rendering: " << e.what() << std::endl;
    }
}

// Render the floor for the test map
void RendererCuda::renderTestMapFloorCuda(const ViewPosition& view) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    try {
        // Check if device buffers are allocated
        if (!m_buffersAllocated || !m_cudaData->d_frameBuffer || !m_cudaData->d_zBuffer) {
            std::cerr << "Error: CUDA buffers not allocated for floor rendering" << std::endl;
            return;
        }
        
        // Get player information
        float playerX = view.position.x;
        float playerY = view.position.y;
        float playerAngle = view.angle;
        float playerHeight = view.height;
        float fov = view.fov;
        
        // Use the maxViewDistance from skybox
        float maxViewDistance = m_skybox.maxViewDistance;
        
        // Find which sector the player is in
        int currentSector = -1;
        float floorHeight = 0.0f;
        float ceilingHeight = 2.0f;
        int floorTextureId = 0;
        int lightLevel = 200;
        
        // Create a host copy of the BSP tree nodes to find the sector
        std::vector<CudaBSPNode> nodes(m_deviceBSPTree.nodeCount);
        cudaError_t err = cudaMemcpy(nodes.data(), m_deviceBSPTree.nodes, 
                                    m_deviceBSPTree.nodeCount * sizeof(CudaBSPNode), 
                                    cudaMemcpyDeviceToHost);
        
        if (err == cudaSuccess) {
            // Simple BSP traversal to find the sector containing the player
            int nodeIndex = m_deviceBSPTree.rootNodeIndex;
            while (nodeIndex >= 0 && nodeIndex < nodes.size()) {
                const CudaBSPNode& node = nodes[nodeIndex];
                
                if (node.isLeaf) {
                    currentSector = node.sectorId;
                    break;
                }
                
                // Calculate which side of the partition line the player is on
                float dx = node.partitioner.end.x - node.partitioner.start.x;
                float dy = node.partitioner.end.y - node.partitioner.start.y;
                float crossProduct = (playerX - node.partitioner.start.x) * dy - 
                                    (playerY - node.partitioner.start.y) * dx;
                
                if (crossProduct >= 0) {
                    nodeIndex = node.frontNodeIndex;
                } else {
                    nodeIndex = node.backNodeIndex;
                }
            }
            
            // If we found a valid sector, get its properties
            if (currentSector >= 0 && currentSector < m_deviceBSPTree.sectorCount) {
                // Create a host copy of the sector data
                std::vector<CudaSector> sectors(m_deviceBSPTree.sectorCount);
                err = cudaMemcpy(sectors.data(), m_deviceBSPTree.sectors, 
                                m_deviceBSPTree.sectorCount * sizeof(CudaSector), 
                                cudaMemcpyDeviceToHost);
                
                if (err == cudaSuccess) {
                    const CudaSector& sector = sectors[currentSector];
                    floorHeight = sector.floorHeight;
                    ceilingHeight = sector.ceilingHeight;
                    floorTextureId = sector.floorTextureId;
                    lightLevel = sector.lightLevel;
                    
                    std::cout << "Player in sector " << currentSector << " with floor height " 
                              << floorHeight << " and ceiling height " << ceilingHeight << std::endl;
                }
            }
        }
        
        // Determine thread block and grid sizes - use 2D grid for floor
        dim3 blockSize(16, 16);  // 16x16 threads per block
        dim3 gridSize(
            (m_width + blockSize.x - 1) / blockSize.x,
            (m_height + blockSize.y - 1) / blockSize.y
        );
        
        // Launch the floor rendering kernel
        floorRenderKernel<<<gridSize, blockSize>>>(
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
            ceilingHeight,
            floorTextureId,
            lightLevel,
            m_cudaData->d_textures,
            m_cudaData->numTextures
        );
        
        // Check for errors
        cudaError_t kernelErr = cudaGetLastError();
        if (kernelErr != cudaSuccess) {
            std::cerr << "CUDA error in test map floor rendering: " << cudaGetErrorString(kernelErr) << std::endl;
        } else {
            // Sync after kernel execution to catch any delayed errors
            kernelErr = cudaDeviceSynchronize();
            if (kernelErr != cudaSuccess) {
                std::cerr << "CUDA sync error after test map floor rendering: " << cudaGetErrorString(kernelErr) << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in renderTestMapFloorCuda: " << e.what() << std::endl;
    }
}

// Add implementation of getNumTextures() method
int RendererCuda::getNumTextures() const {
    if (!m_cudaData) return 0;
    return m_cudaData->numTextures;
}

// Kernel to render platforms
__global__ void renderPlatformsKernel(
    Color* frameBuffer,
    float* zBuffer,
    int width,
    int height,
    CudaPlatform* platforms,
    int platformCount,
    const ViewPosition view,
    CudaRenderData::TextureData* textures
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= width || y >= height) return;
    
    // For each platform
    for (int i = 0; i < platformCount; i++) {
        CudaPlatform& platform = platforms[i];
        
        // Skip if platform has no vertices
        if (platform.vertexCount < 3) continue;
        
        // Calculate the world space position for this pixel
        float rayAngle = view.angle - (view.fov * 0.5f * DEG_TO_RAD) + (x / (float)width) * (view.fov * DEG_TO_RAD);
        float rayDirX = cosf(rayAngle);
        float rayDirY = sinf(rayAngle);
        
        // Check if this ray intersects with the platform
        bool intersects = false;
        float intersectionDistance = FLT_MAX; // Use FLT_MAX for better precision
        float intersectionHeight = 0.0f;
        
        // First, do a quick check if the platform is potentially visible
        // Calculate platform center
        float centerX = 0.0f, centerY = 0.0f;
        for (int j = 0; j < platform.vertexCount; j++) {
            centerX += platform.vertices[j][0];
            centerY += platform.vertices[j][1];
        }
        centerX /= platform.vertexCount;
        centerY /= platform.vertexCount;
        
        // Calculate vector from player to platform center
        float toCenterX = centerX - view.position.x;
        float toCenterY = centerY - view.position.y;
        
        // Calculate distance to platform center
        float distToCenter = sqrtf(toCenterX * toCenterX + toCenterY * toCenterY);
        
        // Skip if platform is too far away
        if (distToCenter > 20.0f) continue;
        
        // Calculate angle to platform center
        float angleToPlatform = atan2f(toCenterY, toCenterX);
        
        // Normalize angles to [0, 2π)
        while (angleToPlatform < 0) angleToPlatform += 2.0f * M_PI;
        float viewAngle = view.angle;
        while (viewAngle < 0) viewAngle += 2.0f * M_PI;
        
        // Calculate angle difference
        float angleDiff = fabsf(angleToPlatform - viewAngle);
        while (angleDiff > M_PI) angleDiff = 2.0f * M_PI - angleDiff;
        
        // We're removing the FOV check to ensure platforms are always rendered
        // regardless of the view angle
        
        // Improved ray-polygon intersection test
        for (int j = 0; j < platform.vertexCount; j++) {
            int k = (j + 1) % platform.vertexCount;
            
            // Edge from vertices[j] to vertices[k]
            float x1 = platform.vertices[j][0];
            float y1 = platform.vertices[j][1];
            float x2 = platform.vertices[k][0];
            float y2 = platform.vertices[k][1];
            
            // Ray equation: view.position + t * rayDir
            // Edge equation: (x1,y1) + s * ((x2,y2) - (x1,y1))
            
            float denominator = (y2 - y1) * rayDirX - (x2 - x1) * rayDirY;
            
            // Use a smaller epsilon for better precision
            if (fabsf(denominator) < 0.000001f) continue; // Parallel
            
            float t = ((x2 - x1) * (view.position.y - y1) - (y2 - y1) * (view.position.x - x1)) / denominator;
            float s = (rayDirX * (view.position.y - y1) - rayDirY * (view.position.x - x1)) / denominator;
            
            if (t >= 0.0f && s >= 0.0f && s <= 1.0f && t < intersectionDistance) {
                // Check if this is the closest intersection
                intersects = true;
                intersectionDistance = t;
                intersectionHeight = platform.height;
            }
        }
        
        if (intersects) {
            // Calculate screen space coordinates for the platform
            float worldX = view.position.x + rayDirX * intersectionDistance;
            float worldY = view.position.y + rayDirY * intersectionDistance;
            
            // Calculate the screen space y-coordinate for the platform top
            float screenY = height / 2.0f - (platform.height - view.height) * DISTANCE_MULTIPLIER / intersectionDistance;
            
            // Calculate the screen space y-coordinate for the platform bottom
            float bottomScreenY = height / 2.0f - (platform.height - platform.thickness - view.height) * DISTANCE_MULTIPLIER / intersectionDistance;
            
            // Only render if the platform is visible and within screen bounds
            if (screenY < bottomScreenY && screenY < height && bottomScreenY >= 0) {
                // Clamp to screen bounds
                int startY = max(0, (int)screenY);
                int endY = min(height - 1, (int)bottomScreenY);
                
                // Calculate texture coordinates with better scaling
                float texScale = 1.0f; // Adjust for texture detail
                float u = fmodf(worldX * texScale, 1.0f);
                if (u < 0.0f) u += 1.0f;
                
                float v = fmodf(worldY * texScale, 1.0f);
                if (v < 0.0f) v += 1.0f;
                
                // Check if texture ID is valid
                if (platform.topTextureId < 0 || platform.topTextureId >= 32) continue; // Avoid invalid texture IDs
                
                // Get the texture
                CudaRenderData::TextureData& texture = textures[platform.topTextureId];
                
                // Render the platform with z-buffer check for each pixel
                for (int py = startY; py <= endY; py++) {
                    // Calculate z-buffer value with consistent approach
                    float depth = intersectionDistance / 20.0f; // Normalize to 0-1 range
                    
                    // Use the same tolerance approach as walls and floors
                    float tolerance = 0.001f + (depth * 0.01f);
                    
                    // Skip if this pixel is behind something else (with tolerance)
                    if (depth >= zBuffer[py * width + x] + tolerance) continue;
                    
                    // Sample the texture
                    int tx = (int)(u * texture.width) % texture.width;
                    int ty = (int)(v * texture.height) % texture.height;
                    Color color = texture.pixels[ty * texture.width + tx];
                    
                    // Apply lighting with minimum brightness
                    float lightFactor = fmaxf(0.3f, platform.lightLevel / 255.0f);
                    color.r = (uint8_t)(color.r * lightFactor);
                    color.g = (uint8_t)(color.g * lightFactor);
                    color.b = (uint8_t)(color.b * lightFactor);
                    
                    // Write to frame buffer
                    frameBuffer[py * width + x] = color;
                    
                    // Update z-buffer
                    zBuffer[py * width + x] = depth;
                }
            }
        }
    }
}

} // namespace PureDoom 