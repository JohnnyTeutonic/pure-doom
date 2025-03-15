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

// Structure for passing data between CPU and GPU
struct CudaRenderData {
    // Device pointers
    Color* d_frameBuffer;
    float* d_zBuffer;
    
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

// Host code for RendererCuda implementation

RendererCuda::RendererCuda(int width, int height) 
    : m_width(width), m_height(height), m_cudaAvailable(false), m_initialized(false), m_cudaData(nullptr) {
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

void RendererCuda::allocateCudaMemory() {
    if (!m_cudaAvailable || !m_cudaData) return;
    
    // Allocate device memory for frame buffer
    CUDA_CHECK(cudaMalloc(&m_cudaData->d_frameBuffer, m_width * m_height * sizeof(Color)));
    
    // Allocate device memory for Z-buffer
    CUDA_CHECK(cudaMalloc(&m_cudaData->d_zBuffer, m_width * m_height * sizeof(float)));
}

void RendererCuda::freeCudaMemory() {
    if (!m_cudaAvailable || !m_cudaData) return;
    
    // Free device memory
    if (m_cudaData->d_frameBuffer) {
        CUDA_CHECK(cudaFree(m_cudaData->d_frameBuffer));
        m_cudaData->d_frameBuffer = nullptr;
    }
    
    if (m_cudaData->d_zBuffer) {
        CUDA_CHECK(cudaFree(m_cudaData->d_zBuffer));
        m_cudaData->d_zBuffer = nullptr;
    }
}

void RendererCuda::prepareForRendering(const std::vector<Color>& frameBuffer, const std::vector<float>& zBuffer) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    // Copy frame buffer data to device
    CUDA_CHECK(cudaMemcpy(m_cudaData->d_frameBuffer, frameBuffer.data(), 
                        m_width * m_height * sizeof(Color), cudaMemcpyHostToDevice));
    
    // Copy Z-buffer data to device
    CUDA_CHECK(cudaMemcpy(m_cudaData->d_zBuffer, zBuffer.data(), 
                        m_width * m_height * sizeof(float), cudaMemcpyHostToDevice));
}

void RendererCuda::retrieveRenderingResults(std::vector<Color>& frameBuffer, std::vector<float>& zBuffer) {
    if (!m_cudaAvailable || !m_initialized || !m_cudaData) return;
    
    // Copy frame buffer data back to host
    CUDA_CHECK(cudaMemcpy(frameBuffer.data(), m_cudaData->d_frameBuffer, 
                        m_width * m_height * sizeof(Color), cudaMemcpyDeviceToHost));
    
    // Copy Z-buffer data back to host
    CUDA_CHECK(cudaMemcpy(zBuffer.data(), m_cudaData->d_zBuffer, 
                        m_width * m_height * sizeof(float), cudaMemcpyDeviceToHost));
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

// These methods are stubs for now - to be implemented in follow-up PR
void RendererCuda::renderBSPCuda(const BSPTree& bsp, const ViewPosition& view, float maxViewDistance) {
    // Will be implemented in next phase
}

void RendererCuda::renderFloorAndCeilingCuda(const BSPTree& bsp, const ViewPosition& view) {
    // Will be implemented in next phase
}

void RendererCuda::renderSpritesCuda(const BSPTree& bsp, const ViewPosition& view, 
                                  const std::vector<Sprite>& sprites) {
    // Will be implemented in next phase
}

} // namespace PureDoom 