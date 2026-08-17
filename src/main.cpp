
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_properties.h>
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

  SDL_DestroyGPUDevice(device);

  SDL_DestroyWindow(w);

  SDL_Quit();
}
