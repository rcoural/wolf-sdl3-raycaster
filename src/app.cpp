// Window, SDL3 GPU pipeline and the per-frame loop.
// The CPU raycaster fills `cpuFB`, which is uploaded to a GPU texture every
// frame and drawn to the screen as a fullscreen textured quad.

#include "app.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <vector>

class SDLException final : public std::runtime_error {
public:
    explicit SDLException(const std::string& message)
        : std::runtime_error(message + '\n' + SDL_GetError()) {}
};

void App::updateCPUFramebuffer() {
    game->castRays(reinterpret_cast<uint32_t*>(cpuFB.data()),
                   fbPitch / 4,
                   fbW, fbH);
}

// Allocate the CPU framebuffer + matching GPU texture and transfer buffer
// used to stream frames to the GPU each tick.
void App::createRaycastFramebuffer(int w, int h) {
    fbW     = w;
    fbH     = h;
    fbPitch = fbW * 4;
    cpuFB.resize(static_cast<size_t>(fbPitch) * fbH);

    const SDL_GPUTextureCreateInfo tci{
        .type                 = SDL_GPU_TEXTURETYPE_2D,
        .format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width                = static_cast<Uint32>(w),
        .height               = static_cast<Uint32>(h),
        .layer_count_or_depth = 1,
        .num_levels           = 1,
    };
    frameTexture = SDL_CreateGPUTexture(device, &tci);
    if (!frameTexture) throw SDLException("Couldn't create frameTexture");
    SDL_SetGPUTextureName(device, frameTexture, "CPU_Framebuffer");

    const SDL_GPUTransferBufferCreateInfo tbci{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size  = static_cast<Uint32>(fbPitch) * static_cast<Uint32>(fbH),
    };
    frameTransfer = SDL_CreateGPUTransferBuffer(device, &tbci);
    if (!frameTransfer) throw SDLException("Couldn't create frameTransfer");
}

// Map the transfer buffer, copy CPU pixels in, then submit a copy pass that
// uploads them into the sampleable GPU texture.
void App::uploadFramebufferToGPU() {
    auto* dst = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device, frameTransfer, false));
    if (!dst) throw SDLException("Map frameTransfer failed");
    std::memcpy(dst, cpuFB.data(), static_cast<size_t>(fbPitch) * fbH);
    SDL_UnmapGPUTransferBuffer(device, frameTransfer);

    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(device);
    if (!cmdbuf) throw SDLException("Acquire cmd buffer (upload) failed");

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmdbuf);

    const SDL_GPUTextureTransferInfo tx{
        .transfer_buffer = frameTransfer,
    };

    const SDL_GPUTextureRegion region{
        .texture = frameTexture,
        .x = 0, .y = 0, .z = 0,
        .w = static_cast<Uint32>(fbW),
        .h = static_cast<Uint32>(fbH),
        .d = 1,
    };

    SDL_UploadToGPUTexture(copy, &tx, &region, false);
    SDL_EndGPUCopyPass(copy);

    if (!SDL_SubmitGPUCommandBuffer(cmdbuf)) {
        throw SDLException("Submit cmd buffer (upload) failed");
    }
}

std::string App::BasePath{};

void App::InitializeAssetLoader() {
    BasePath = std::string{ SDL_GetBasePath() };
}

bool App::init() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        throw SDLException("SDL_Init failed");

    InitializeAssetLoader();

    window = SDL_CreateWindow("Raycaster", WIN_W, WIN_H, 0);
    if (!window)
        throw SDLException("SDL_CreateWindow failed");

    device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_DXIL,
        true, nullptr);
    if (!device)
        throw SDLException("SDL_CreateGPUDevice failed");

    if (!SDL_ClaimWindowForGPUDevice(device, window))
        throw SDLException("SDL_ClaimWindowForGPUDevice failed");

    createRaycastFramebuffer(WIN_W, WIN_H);

    // Shaders just sample a 2D texture onto a fullscreen quad.
    SDL_GPUShader* vertexShader = LoadShader(device, "TexturedQuad.vert", 0, 0, 0, 0);
    if (!vertexShader) throw SDLException("Couldn't load vertex shader");

    SDL_GPUShader* fragmentShader = LoadShader(device, "TexturedQuad.frag", 1, 0, 0, 0);
    if (!fragmentShader) throw SDLException("Couldn't load fragment shader");

    const SDL_GPUColorTargetDescription colorTargetDescription{
        .format = SDL_GetGPUSwapchainTextureFormat(device, window),
    };
    const std::vector colorTargetDescriptions{ colorTargetDescription };

    const SDL_GPUGraphicsPipelineTargetInfo targetInfo{
        .color_target_descriptions = colorTargetDescriptions.data(),
        .num_color_targets         = static_cast<Uint32>(colorTargetDescriptions.size()),
    };

    const std::vector<SDL_GPUVertexAttribute> vertexAttributes{
        { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex, position) },
        { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, uv) },
    };
    const std::vector<SDL_GPUVertexBufferDescription> vertexBufferDescriptions{
        { 0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 },
    };

    const SDL_GPUVertexInputState vertexInputState{
        .vertex_buffer_descriptions = vertexBufferDescriptions.data(),
        .num_vertex_buffers         = static_cast<Uint32>(vertexBufferDescriptions.size()),
        .vertex_attributes          = vertexAttributes.data(),
        .num_vertex_attributes      = static_cast<Uint32>(vertexAttributes.size()),
    };

    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo{
        .vertex_shader      = vertexShader,
        .fragment_shader    = fragmentShader,
        .vertex_input_state = vertexInputState,
        .primitive_type     = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    };
    pipelineCreateInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipelineCreateInfo.target_info                = targetInfo;

    pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo);
    if (!pipeline) throw SDLException("PipeLine failed");

    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, fragmentShader);

    const SDL_GPUSamplerCreateInfo samplerInfo{
        .min_filter     = SDL_GPU_FILTER_LINEAR,
        .mag_filter     = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode    = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    sampler = SDL_CreateGPUSampler(device, &samplerInfo);
    if (!sampler) throw SDLException("GPU Sampler failed");

    // Fullscreen quad in clip space; UVs flipped vertically so the framebuffer
    // appears upright on screen.
    vertices = {
        {{-1.f, -1.f, 0.f}, {0.f, 1.f}},
        {{ 1.f, -1.f, 0.f}, {1.f, 1.f}},
        {{ 1.f,  1.f, 0.f}, {1.f, 0.f}},
        {{-1.f,  1.f, 0.f}, {0.f, 0.f}},
    };
    indices = { 0, 1, 2, 0, 2, 3 };

    const SDL_GPUBufferCreateInfo vertexBufferCreateInfo{
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size  = static_cast<Uint32>(vertices.size() * sizeof(Vertex)),
    };
    vertexBuffer = SDL_CreateGPUBuffer(device, &vertexBufferCreateInfo);
    if (!vertexBuffer) throw SDLException("Couldn't create GPU buffer");
    SDL_SetGPUBufferName(device, vertexBuffer, "Vertex Buffer");

    const SDL_GPUBufferCreateInfo indexBufferCreateInfo{
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size  = static_cast<Uint32>(indices.size() * sizeof(Uint32)),
    };
    indexBuffer = SDL_CreateGPUBuffer(device, &indexBufferCreateInfo);
    if (!indexBuffer) throw SDLException("Couldn't create GPU buffer");
    SDL_SetGPUBufferName(device, indexBuffer, "Index Buffer");

    const SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size  = vertexBufferCreateInfo.size + indexBufferCreateInfo.size,
    };
    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);
    if (!transferBuffer) throw SDLException("Couldn't create transfer buffer");

    auto* transferBufferDataPtr = static_cast<Uint8*>(SDL_MapGPUTransferBuffer(device, transferBuffer, false));
    if (!transferBufferDataPtr) throw SDLException("Couldn't map transfer buffer");

    std::span vertexBufferData{ reinterpret_cast<Vertex*>(transferBufferDataPtr), vertices.size() };
    std::ranges::copy(vertices, vertexBufferData.begin());

    std::span indexBufferData{ reinterpret_cast<Uint32*>(transferBufferDataPtr + vertexBufferCreateInfo.size), indices.size() };
    std::ranges::copy(indices, indexBufferData.begin());

    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    SDL_GPUCommandBuffer* transferCommandBuffer = SDL_AcquireGPUCommandBuffer(device);
    if (!transferCommandBuffer) throw SDLException("Couldn't acquire GPU command buffer");

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(transferCommandBuffer);

    SDL_GPUTransferBufferLocation source{
        .transfer_buffer = transferBuffer,
        .offset          = 0,
    };
    SDL_GPUBufferRegion destination{
        .buffer = vertexBuffer,
        .offset = 0,
        .size   = vertexBufferCreateInfo.size,
    };
    SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

    source.offset      = vertexBufferCreateInfo.size;
    destination.buffer = indexBuffer;
    destination.offset = 0;
    destination.size   = indexBufferCreateInfo.size;
    SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

    SDL_EndGPUCopyPass(copyPass);
    if (!SDL_SubmitGPUCommandBuffer(transferCommandBuffer))
        throw SDLException("Couldn't submit GPU command buffer");

    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);

    SDL_ShowWindow(window);
    SDL_SetWindowRelativeMouseMode(window, true);

    game = std::make_unique<Game>();
    if (!game->loadAssets(BasePath)) {
        std::cerr << "Missing texture BMPs.\n";
        return false;
    }
    return true;
}

void App::update() {
    int numkeys = 0;
    const bool* keys = SDL_GetKeyboardState(&numkeys);
    game->handleInput({ keys, static_cast<size_t>(numkeys) });
}

void App::render() {
    SDL_GPUTexture* swapchainTexture = nullptr;

    // FPS report.
    const uint64_t perfNow = SDL_GetPerformanceCounter();
    const uint64_t freq    = SDL_GetPerformanceFrequency();
    const float deltaSeconds = (perfNow - perfLast) / static_cast<float>(freq);
    const float currentFPS   = 1.0f / deltaSeconds;
    perfLast = perfNow;
    std::cout << std::format("FPS: {:.1f}\n", currentFPS);

    // 1) cast rays into the CPU framebuffer, 2) upload to the GPU texture.
    updateCPUFramebuffer();
    uploadFramebufferToGPU();

    SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(device);
    if (!commandBuffer) {
        SDL_Log("SDL_AcquireGPUCommandBuffer: %s", SDL_GetError());
    }

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, window, &swapchainTexture, nullptr, nullptr)) {
        SDL_Log("SDL_WaitAndAcquireGPUSwapchainTexture: %s", SDL_GetError());
    }

    if (swapchainTexture) {
        // 3) render pass: clear, draw the textured quad sampling frameTexture.
        const SDL_GPUColorTargetInfo colorTargetInfo{
            .texture     = swapchainTexture,
            .clear_color = SDL_FColor{ 0.1f, 0.1f, 0.1f, 1.0f },
            .load_op     = SDL_GPU_LOADOP_CLEAR,
            .store_op    = SDL_GPU_STOREOP_STORE,
        };
        const std::vector colorTargets{ colorTargetInfo };

        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(
            commandBuffer, colorTargets.data(), static_cast<Uint32>(colorTargets.size()), nullptr);

        const std::vector<SDL_GPUBufferBinding> bindings{ { vertexBuffer, 0 } };
        SDL_BindGPUGraphicsPipeline(renderPass, pipeline);
        SDL_BindGPUVertexBuffers(renderPass, 0, bindings.data(), static_cast<Uint32>(bindings.size()));

        const SDL_GPUBufferBinding indexBufferBinding{ indexBuffer, 0 };
        SDL_BindGPUIndexBuffer(renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

        const SDL_GPUTextureSamplerBinding textureSamplerBinding{
            .texture = frameTexture,
            .sampler = sampler,
        };
        const std::vector textureSamplerBindings{ textureSamplerBinding };
        SDL_BindGPUFragmentSamplers(renderPass, 0, textureSamplerBindings.data(), static_cast<Uint32>(textureSamplerBindings.size()));

        SDL_DrawGPUIndexedPrimitives(renderPass, static_cast<Uint32>(indices.size()), 1, 0, 0, 0);
        SDL_EndGPURenderPass(renderPass);

        if (!SDL_SubmitGPUCommandBuffer(commandBuffer)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer: %s", SDL_GetError());
        }
    }
}

void App::handleEvent(const SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT) {
        running = false;
    }
    else if (e.type == SDL_EVENT_KEY_UP && e.key.key == SDLK_ESCAPE) {
        running = false;
    }
    else if (e.type == SDL_EVENT_MOUSE_MOTION) {
        game->handleMouseMotion(e.motion.xrel);
    }

    if (e.type == SDL_EVENT_KEY_DOWN) {
        if ((e.key.key == SDLK_RETURN) && (e.key.mod & SDL_KMOD_CTRL)) {
            const Uint32 flags = SDL_GetWindowFlags(window);
            if (flags & SDL_WINDOW_FULLSCREEN) {
                SDL_SetWindowFullscreen(window, 0);
            }
            else {
                SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
            }
        }
    }
}

void App::shutdown() {
    if (game) {
        game->unloadAssets();
        game.reset();
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
}

// Loads a precompiled shader binary picked by the active GPU backend
// (SPIR-V on Vulkan, MSL on Metal, DXIL on D3D12).
SDL_GPUShader* App::LoadShader(
    SDL_GPUDevice* device,
    const std::string& shaderFilename,
    const Uint32 samplerCount,
    const Uint32 uniformBufferCount,
    const Uint32 storageBufferCount,
    const Uint32 storageTextureCount)
{
    SDL_GPUShaderStage stage;
    if (shaderFilename.contains(".vert")) {
        stage = SDL_GPU_SHADERSTAGE_VERTEX;
    }
    else if (shaderFilename.contains(".frag")) {
        stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    }
    else {
        throw std::runtime_error("Unrecognized shader stage!");
    }

    std::string fullPath;
    const SDL_GPUShaderFormat backendFormats = SDL_GetGPUShaderFormats(device);
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
    const char* entrypoint = nullptr;

    if (backendFormats & SDL_GPU_SHADERFORMAT_SPIRV) {
        fullPath   = std::format("{}/shaders/{}.spv", BasePath, shaderFilename);
        format     = SDL_GPU_SHADERFORMAT_SPIRV;
        entrypoint = "main";
    }
    else if (backendFormats & SDL_GPU_SHADERFORMAT_MSL) {
        fullPath   = std::format("{}/shaders/{}.msl", BasePath, shaderFilename);
        format     = SDL_GPU_SHADERFORMAT_MSL;
        entrypoint = "main0";
    }
    else if (backendFormats & SDL_GPU_SHADERFORMAT_DXIL) {
        fullPath   = std::format("{}/shaders/{}.dxil", BasePath, shaderFilename);
        format     = SDL_GPU_SHADERFORMAT_DXIL;
        entrypoint = "main";
    }
    else {
        SDL_Log("%s", "Unrecognized backend shader format!");
        return nullptr;
    }

    std::ifstream file(fullPath, std::ios::binary);
    if (!file)
        throw std::runtime_error("Couldn't open shader file");
    const std::vector<Uint8> code{ std::istreambuf_iterator<char>(file), {} };

    const SDL_GPUShaderCreateInfo shaderInfo{
        .code_size            = code.size(),
        .code                 = code.data(),
        .entrypoint           = entrypoint,
        .format               = format,
        .stage                = stage,
        .num_samplers         = samplerCount,
        .num_storage_textures = storageTextureCount,
        .num_storage_buffers  = storageBufferCount,
        .num_uniform_buffers  = uniformBufferCount,
    };

    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shaderInfo);
    if (!shader) {
        throw SDLException("Couldn't create GPU shader");
    }
    return shader;
}
