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
    
    // Comprehensive cleanup of all resources
    void cleanup();
    
    // Memory management functions
    void allocateBuffers();  // Allocate GPU buffers if not already allocated
    void freeBuffers();      // Free GPU buffers
    void clearBuffers();     // Clear frameBuffer and zBuffer on GPU
    
    // Copy data from host to device for rendering
    void prepareForRendering(const std::vector<Color>& frameBuffer, const std::vector<float>& zBuffer);
    
    // Copy rendering results back from device to host
    void retrieveRenderingResults(std::vector<Color>& frameBuffer, std::vector<float>& zBuffer);
    
    // Upload textures to the GPU
    void uploadTextures(const std::vector<Texture>& textures);

    // BSP serialization for CUDA
    void serializeBSPForCuda(const BSPTree& bsp);
    void freeBSPData();
    
    // Set the reference to the skybox (needed for maxViewDistance)
    void setSkybox(const Skybox& skybox) { m_skybox = skybox; }
    
    // Render a complete frame on the GPU
    void renderFrame(const BSPTree& bsp, const ViewPosition& view, 
                    const std::vector<Sprite>& sprites, float deltaTime);
    
    // CUDA accelerated rendering functions
    void renderSkyboxCuda(const ViewPosition& view, float deltaTime, const Skybox& skybox);
    void drawSunCuda(float screenX, float screenY, float sizeDegrees, const Color& color, 
                    float intensity, const Skybox& skybox);
    
    // CUDA accelerated BSP rendering
    void renderBSPCuda(const BSPTree& bsp, const ViewPosition& view, float maxViewDistance);
    
    // CUDA accelerated floor rendering (ceiling is now handled by skybox)
    void renderFloorCuda(const BSPTree& bsp, const ViewPosition& view);
    
    // CUDA accelerated sprite rendering
    void renderSpritesCuda(const BSPTree& bsp, const ViewPosition& view, 
                         const std::vector<Sprite>& sprites);
    
    // Check if we have a CUDA-capable device
    bool isCudaAvailable() const { return m_cudaAvailable; }
    
    // Check if textures need to be re-uploaded
    bool needsTextureReUpload() const { return !m_texturesUploaded; }
    
private:
    int m_width;
    int m_height;
    bool m_cudaAvailable;
    bool m_initialized;
    bool m_buffersAllocated;  // Track if GPU buffers are allocated
    bool m_texturesUploaded;  // Track if textures are uploaded
    bool m_bspUploaded;       // Track if BSP tree is uploaded
    CudaDeviceInfo m_deviceInfo;
    Skybox m_skybox;  // Store a copy of the skybox for reference
    
    // CUDA data structure containing device pointers and state
    // This is a pointer to avoid exposing CUDA types in the header
    CudaRenderData* m_cudaData;
    
    // Serialized BSP data on device
    CudaBSPTree m_deviceBSPTree;
    
    // Allocate CUDA memory
    void allocateCudaMemory();
    
    // Free CUDA memory
    void freeCudaMemory();
};

} // namespace PureDoom

#endif // RENDERER_CUDA_H 