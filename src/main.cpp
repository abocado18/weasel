
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_video.h>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl3.h"
#include "imgui/imgui_impl_sdlgpu3.h"

int main() {

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    std::cout << "Could not Init SDL" << std::endl;
    std::abort();
  }

  std::cout << "Init SDL" << std::endl;

  SDL_Window *w = SDL_CreateWindow("Weasel", 1280, 720, SDL_WINDOW_RESIZABLE);

  if (!w) {
    std::cout << "Could not create Window" << std::endl;
    std::abort();
  }

  std::cout << "Created Window" << std::endl;

#ifdef NDEBUG
  bool debug = false;
  std::cout << "Release Build: Disable Validation" << std::endl;
#else
  bool debug = true;
  std::cout << "Debug Build: Enable Validation" << std::endl;
#endif

  SDL_GPUDevice *device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV |
                                                  SDL_GPU_SHADERFORMAT_DXBC |
                                                  SDL_GPU_SHADERFORMAT_DXIL,
                                              debug, 0);

  if (!device) {
    std::cout << "Could not create device" << std::endl;
    std::abort();
  }

  std::cout << "Device created" << std::endl;

  SDL_PropertiesID deviceId = SDL_GetGPUDeviceProperties(device);

  std::cout << "Use Gpu: "
            << SDL_GetStringProperty(deviceId, SDL_PROP_GPU_DEVICE_NAME_STRING,
                                     "unknown")
            << std::endl;

  std::cout << "Use Backend: " << SDL_GetGPUDeviceDriver(device) << std::endl;

  bool running = true;

  SDL_ClaimWindowForGPUDevice(device, w);

  SDL_GPUPresentMode presentMode = [&]() -> SDL_GPUPresentMode {
    if (SDL_WindowSupportsGPUPresentMode(
            device, w, SDL_GPUPresentMode::SDL_GPU_PRESENTMODE_MAILBOX)) {

      std::cout << "Use Present Mode Mailbox" << std::endl;

      return SDL_GPUPresentMode::SDL_GPU_PRESENTMODE_MAILBOX;
    } else {
      std::cout << "Use Present Mode Fifo" << std::endl;
      return SDL_GPUPresentMode::SDL_GPU_PRESENTMODE_VSYNC;
    }
  }();

  SDL_SetGPUSwapchainParameters(
      device, w, SDL_GPUSwapchainComposition::SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
      presentMode);

  if (!SDL_SetGPUAllowedFramesInFlight(device, 3)) {
    std::cout << "Could not set Frames in Flight to 3, default to 2"
              << std::endl;
  } else {
    std::cout << "Set Frames in Flight to 3" << std::endl;
  }

  // ImGui

  {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplSDL3_InitForSDLGPU(w);
    ImGui_ImplSDLGPU3_InitInfo initInfo{};
    initInfo.Device = device;
    initInfo.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, w);
    initInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    initInfo.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
    initInfo.PresentMode = presentMode;
    ImGui_ImplSDLGPU3_Init(&initInfo);
  }

  SDL_GPUGraphicsPipeline *graphicsPipeline;
  {
    SDL_GPUShader *vertexShader;

    {
      SDL_IOStream *vertexStream = SDL_IOFromFile(
          "/home/jan/Dokumente/Projects/weasel/shaders/triangle.vs.spv", "rb");

      int64_t size = SDL_GetIOSize(vertexStream);
      std::vector<std::byte> vertexData(size);
      SDL_ReadIO(vertexStream, vertexData.data(), size);

      SDL_CloseIO(vertexStream);

      SDL_GPUShaderCreateInfo createInfo{
          .code_size = vertexData.size(),
          .code = reinterpret_cast<uint8_t *>(vertexData.data()),
          .entrypoint = "vertexMain",
          .format = SDL_GPU_SHADERFORMAT_SPIRV,
          .stage = SDL_GPU_SHADERSTAGE_VERTEX,

      };

      vertexShader = SDL_CreateGPUShader(device, &createInfo);

      assert(vertexShader);
    }

    SDL_GPUShader *pixelShader;

    {
      SDL_IOStream *pixelStream = SDL_IOFromFile(
          "/home/jan/Dokumente/Projects/weasel/shaders/triangle.ps.spv", "rb");

      int64_t size = SDL_GetIOSize(pixelStream);
      std::vector<std::byte> pixelData(size);
      SDL_ReadIO(pixelStream, pixelData.data(), size);

      SDL_CloseIO(pixelStream);

      SDL_GPUShaderCreateInfo createInfo{
          .code_size = pixelData.size(),
          .code = reinterpret_cast<uint8_t *>(pixelData.data()),
          .entrypoint = "pixelMain",
          .format = SDL_GPU_SHADERFORMAT_SPIRV,
          .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,

      };

      pixelShader = SDL_CreateGPUShader(device, &createInfo);

      assert(pixelShader);
    }

    SDL_GPUVertexAttribute vertexAttribute{
        .location = 0,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
        .offset = 0,
    };

    SDL_GPUVertexBufferDescription vertexBufferDesc{
        .slot = 0,
        .pitch = 16,
        .input_rate = SDL_GPUVertexInputRate::SDL_GPU_VERTEXINPUTRATE_VERTEX};

    SDL_GPUColorTargetDescription colorTargetDesc{
        .format = SDL_GetGPUSwapchainTextureFormat(device, w),
        .blend_state = SDL_GPUColorTargetBlendState{
            .enable_blend = false,
            .enable_color_write_mask = false,

        }};

    SDL_GPUGraphicsPipelineCreateInfo createInfo{
        .vertex_shader = vertexShader,
        .fragment_shader = pixelShader,
        .vertex_input_state =
            SDL_GPUVertexInputState{
                .vertex_buffer_descriptions = &vertexBufferDesc,
                .num_vertex_buffers = 1,
                .vertex_attributes = &vertexAttribute,
                .num_vertex_attributes = 1,
            },
        .primitive_type =
            SDL_GPUPrimitiveType::SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state =
            SDL_GPURasterizerState{
                .fill_mode = SDL_GPU_FILLMODE_FILL,
                .cull_mode = SDL_GPU_CULLMODE_NONE,
            },
        .multisample_state =
            SDL_GPUMultisampleState{
                .sample_count = SDL_GPUSampleCount::SDL_GPU_SAMPLECOUNT_1,
                .enable_alpha_to_coverage = false,

            },

        .depth_stencil_state =
            SDL_GPUDepthStencilState{
                .enable_depth_test = false,
                .enable_depth_write = false,
                .enable_stencil_test = false,
            },
        .target_info = SDL_GPUGraphicsPipelineTargetInfo{
            .color_target_descriptions = &colorTargetDesc,

            .num_color_targets = 1,

            .has_depth_stencil_target = false}};

    graphicsPipeline = SDL_CreateGPUGraphicsPipeline(device, &createInfo);

    SDL_ReleaseGPUShader(device, vertexShader);
    SDL_ReleaseGPUShader(device, pixelShader);

    assert(graphicsPipeline);
  }

  SDL_GPUBuffer *vertexBuffer;
  {

    SDL_GPUBufferCreateInfo createInfo{
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = 3 * 16,

    };

    vertexBuffer = SDL_CreateGPUBuffer(device, &createInfo);
    assert(vertexBuffer);

    SDL_GPUTransferBuffer *transferBuffer;

    SDL_GPUTransferBufferCreateInfo transferCreateInfo{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = 3 * 16,
    };

    transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferCreateInfo);
    assert(transferBuffer);

    void *ptr = SDL_MapGPUTransferBuffer(device, transferBuffer, false);

    std::array<std::array<float, 4>, 3> vertexData{
        std::array<float, 4>{-0.5f, -0.5f, 0.0f, 1.0f},
        std::array<float, 4>{0.5f, -0.5f, 0.0f, 1.0f},
        std::array<float, 4>{0.0f, 0.5f, 0.0f, 1.0f}};

    memcpy(ptr, vertexData.data(), sizeof(vertexData));

    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);

    SDL_GPUCopyPass *copyPass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUBufferRegion target{.buffer = vertexBuffer, .offset = 0, .size = 48};

    SDL_GPUTransferBufferLocation source{.transfer_buffer = transferBuffer,
                                         .offset = 0};

    SDL_UploadToGPUBuffer(copyPass, &source, &target, false);

    SDL_EndGPUCopyPass(copyPass);

    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);

    SDL_WaitForGPUFences(device, true, &fence, 1);

    SDL_ReleaseGPUFence(device, fence);

    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
  }

  while (running) {

    SDL_Event event;

    while (SDL_PollEvent(&event)) {

      ImGui_ImplSDL3_ProcessEvent(&event);

      switch (event.type) {

      case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {

        running = false;
        break;
      }
      }
    }

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);

    SDL_GPUTexture *swapchainTexture;
    uint32_t width, height;

    SDL_WaitAndAcquireGPUSwapchainTexture(cmd, w, &swapchainTexture, &width,
                                          &height);

    {
      SDL_GPUColorTargetInfo colorTargetInfo{
          .texture = swapchainTexture,
          .mip_level = 0,
          .layer_or_depth_plane = 0,
          .clear_color = SDL_FColor{0.f, 1.f, 1.f, 0.0f},
          .load_op = SDL_GPULoadOp::SDL_GPU_LOADOP_CLEAR,
          .store_op = SDL_GPUStoreOp::SDL_GPU_STOREOP_STORE,
          .cycle = true,
          .cycle_resolve_texture = false,

      };

      SDL_GPURenderPass *renderpass =
          SDL_BeginGPURenderPass(cmd, &colorTargetInfo, 1, nullptr);

      {
        SDL_BindGPUGraphicsPipeline(renderpass, graphicsPipeline);

        SDL_GPUBufferBinding bufferBinding{
            .buffer = vertexBuffer,
            .offset = 0,
        };

        SDL_BindGPUVertexBuffers(renderpass, 0, &bufferBinding, 1);

        SDL_DrawGPUPrimitives(renderpass, 3, 1, 0, 0);

      }

      SDL_EndGPURenderPass(renderpass);
    }

    {

      ImGui_ImplSDLGPU3_NewFrame();
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();

      ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                   ImGuiDockNodeFlags_PassthruCentralNode);
      {
      }

      ImGui::Render();

      ImDrawData *drawData = ImGui::GetDrawData();

      ImGui_ImplSDLGPU3_PrepareDrawData(drawData, cmd);

      // Setup and start a render pass
      SDL_GPUColorTargetInfo target_info = {};
      target_info.texture = swapchainTexture;

      target_info.load_op = SDL_GPU_LOADOP_LOAD;
      target_info.store_op = SDL_GPU_STOREOP_STORE;
      target_info.mip_level = 0;
      target_info.layer_or_depth_plane = 0;
      target_info.cycle = false;
      SDL_GPURenderPass *renderPass =
          SDL_BeginGPURenderPass(cmd, &target_info, 1, nullptr);

      // Render ImGui
      ImGui_ImplSDLGPU3_RenderDrawData(drawData, cmd, renderPass);

      SDL_EndGPURenderPass(renderPass);
    }

    SDL_SubmitGPUCommandBuffer(cmd);
  }

  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();

  SDL_ReleaseGPUGraphicsPipeline(device, graphicsPipeline);

  SDL_DestroyGPUDevice(device);

  SDL_DestroyWindow(w);

  SDL_Quit();
}
