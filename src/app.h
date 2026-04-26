#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

#include "game.h"

struct Vertex {
    glm::vec3 position;
    glm::vec2 uv;
};

class App {
public:
    App() = default;
    ~App() = default;

    App(const App&)            = delete;
    App& operator=(const App&) = delete;
    App(App&&)                 = delete;
    App& operator=(App&&)      = delete;

    bool init();
    void update();
    void render();
    void handleEvent(const SDL_Event& e);
    void shutdown();
    [[nodiscard]] bool isRunning() const noexcept { return running; }

    static void InitializeAssetLoader();
    static std::string BasePath;

private:
    SDL_Window*              window       = nullptr;
    SDL_GPUDevice*           device       = nullptr;
    SDL_GPUGraphicsPipeline* pipeline     = nullptr;
    SDL_GPUSampler*          sampler      = nullptr;
    SDL_GPUBuffer*           vertexBuffer = nullptr;
    SDL_GPUBuffer*           indexBuffer  = nullptr;

    std::vector<Vertex> vertices;
    std::vector<Uint32> indices;

    int fbW     = 0;
    int fbH     = 0;
    int fbPitch = 0;
    std::vector<Uint8>     cpuFB;
    SDL_GPUTexture*        frameTexture  = nullptr;
    SDL_GPUTransferBuffer* frameTransfer = nullptr;

    uint64_t perfLast = 0;

    std::unique_ptr<Game> game;
    bool running = true;

    void uploadFramebufferToGPU();
    void updateCPUFramebuffer();
    void createRaycastFramebuffer(int w, int h);

    SDL_GPUShader* LoadShader(
        SDL_GPUDevice* device,
        const std::string& shaderFilename,
        Uint32 samplerCount,
        Uint32 uniformBufferCount,
        Uint32 storageBufferCount,
        Uint32 storageTextureCount);
};
