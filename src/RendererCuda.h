#ifndef RENDERER_CUDA_H
#define RENDERER_CUDA_H

#include "Renderer.h"
#include "CudaUtils.h"

// Explicitly include necessary CUDA headers
#ifdef __CUDACC__
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#endif

namespace PureDoom {

// Forward declarations
struct CudaRenderData;

// CUDA-accelerated renderer implementation
class RendererCuda {
public:
    RendererCuda(int width, int height);
    ~RendererCuda();
    
    // Initialize the CUDA renderer
    bool initialize();
    
    // Copy data from host to device for rendering
    void prepareForRendering(const std::vector<Color>& frameBuffer, const std::vector<float>& zBuffer);
    
    // Copy rendering results back from device to host
    void retrieveRenderingResults(std::vector<Color>& frameBuffer, std::vector<float>& zBuffer);
    
    // CUDA accelerated rendering functions
    void renderSkyboxCuda(const ViewPosition& view, float deltaTime, const Skybox& skybox);
    void drawSunCuda(float screenX, float screenY, float sizeDegrees, const Color& color, 
                    float intensity, const Skybox& skybox);
    
    // CUDA accelerated BSP rendering
    void renderBSPCuda(const BSPTree& bsp, const ViewPosition& view, float maxViewDistance);
    
    // CUDA accelerated floor and ceiling rendering
    void renderFloorAndCeilingCuda(const BSPTree& bsp, const ViewPosition& view);
    
    // CUDA accelerated sprite rendering
    void renderSpritesCuda(const BSPTree& bsp, const ViewPosition& view, 
                         const std::vector<Sprite>& sprites);
    
    // Check if we have a CUDA-capable device
    bool isCudaAvailable() const { return m_cudaAvailable; }
    
private:
    int m_width;
    int m_height;
    bool m_cudaAvailable;
    bool m_initialized;
    CudaDeviceInfo m_deviceInfo;
    
    // CUDA data structure containing device pointers and state
    // This is a pointer to avoid exposing CUDA types in the header
    CudaRenderData* m_cudaData;
    
    // Allocate CUDA memory
    void allocateCudaMemory();
    
    // Free CUDA memory
    void freeCudaMemory();
};

} // namespace PureDoom

#endif // RENDERER_CUDA_H 