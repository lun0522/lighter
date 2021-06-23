//
//  util.cc
//
//  Created by Pujun Lun on 6/7/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/util.h"

#include <cstdlib>

#include "lighter/common/util.h"

ABSL_FLAG(bool, ignore_vsync, false,
          "Ignore VSync and present images to the screen as fast as possible");

namespace lighter::renderer::util {
namespace {

void InitVulkan() {
  using common::file::GetVulkanSdkPath;

  // TODO: Limit framerate on Windows. Present mode need to be changed as well.
#ifdef __APPLE__
  // TODO: Revisit this.
  if (absl::GetFlag(FLAGS_ignore_vsync)) {
    // To avoid the frame rate being clamped on MacOS when using MoltenVK:
    // https://github.com/KhronosGroup/MoltenVK/issues/581#issuecomment-487293665
    setenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS", "0", /*overwrite=*/1);
    setenv("MVK_CONFIG_PRESENT_WITH_COMMAND_BUFFER", "0", /*overwrite=*/1);
  }

  // export DYLD_LIBRARY_PATH=$VULKAN_SDK/lib:$DYLD_LIBRARY_PATH
  const std::string dyld_lib_path =
      GetVulkanSdkPath("lib") + ":$DYLD_LIBRARY_PATH";
  setenv("DYLD_LIBRARY_PATH", dyld_lib_path.c_str(), /*overwrite=*/1);

  // export VK_ICD_FILENAMES=$VULKAN_SDK/etc/vulkan/icd.d/MoltenVK_icd.json
  setenv("VK_ICD_FILENAMES",
         GetVulkanSdkPath("share/vulkan/icd.d/MoltenVK_icd.json").c_str(),
         /*overwrite=*/1);

  // export VK_LAYER_PATH=$VULKAN_SDK/etc/vulkan/explicit_layer.d
  setenv("VK_LAYER_PATH",
         GetVulkanSdkPath("share/vulkan/explicit_layer.d").c_str(),
         /*overwrite=*/1);
#endif  // __APPLE__

#ifdef __linux__
  // export PATH=$VULKAN_SDK/bin:$PATH
  const std::string path = GetVulkanSdkPath("bin") + ":$PATH";
  setenv("PATH", path.c_str(), /*overwrite=*/1);

  // export LD_LIBRARY_PATH=$VULKAN_SDK/lib
  setenv("LD_LIBRARY_PATH", GetVulkanSdkPath("lib").c_str(), /*overwrite=*/1);

  // export VK_LAYER_PATH=$VULKAN_SDK/etc/vulkan/explicit_layer.d
  setenv("VK_LAYER_PATH",
         GetVulkanSdkPath("etc/vulkan/explicit_layer.d").c_str(),
         /*overwrite=*/1);
#endif  // __linux__
}

}  // namespace

void GlobalInit(common::api::GraphicsApi graphics_api) {
  switch (graphics_api) {
    case common::api::GraphicsApi::kOpengl:
      FATAL("Not implemented yet");

    case common::api::GraphicsApi::kVulkan:
      InitVulkan();
      break;
  }
}

std::unique_ptr<Renderer> CreateRenderer(
    common::api::GraphicsApi graphics_api, const char* application_name,
    const std::optional<debug_message::Config>& debug_message_config,
    std::vector<const common::Window*>&& window_ptrs) {
  switch (graphics_api) {
    case common::api::GraphicsApi::kOpengl:
      FATAL("Not implemented yet");

    case common::api::GraphicsApi::kVulkan:
#ifdef USE_VULKAN
      return std::make_unique<vk::Renderer>(
          application_name, debug_message_config, std::move(window_ptrs));
#else
      FATAL("Vulkan is not enabled");
#endif  // USE_VULKAN
  }
}

}  // namespace lighter::renderer::util
