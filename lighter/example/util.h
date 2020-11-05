//
//  util.h
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_EXAMPLE_UTIL_H
#define LIGHTER_EXAMPLE_UTIL_H

#include <cstdlib>
#include <exception>
#include <memory>
#include <utility>

#include "lighter/common/file.h"
#include "lighter/common/util.h"
#include "lighter/renderer/renderer.h"
#include "lighter/renderer/vk/renderer.h"
#include "third_party/absl/memory/memory.h"

namespace lighter {
namespace example {

enum class Backend { kOpenGL, kVulkan };

std::unique_ptr<renderer::Renderer> CreateRenderer(Backend backend) {
  switch (backend) {
    case Backend::kOpenGL:
      FATAL("Not implemented yet");

    case Backend::kVulkan:
      return absl::make_unique<renderer::vk::Renderer>();
  }
}

template <typename ExampleType, typename... ExampleArgs>
int ExampleMain(int argc, char* argv[], ExampleArgs&&... example_args) {
#ifdef USE_VULKAN
  // Set up the path to find Vulkan SDK.
  using common::file::GetVulkanSdkPath;

#if defined(__APPLE__)
  std::setenv("VK_ICD_FILENAMES",
              GetVulkanSdkPath("share/vulkan/icd.d/MoltenVK_icd.json").c_str(),
              /*overwrite=*/1);
#ifndef NDEBUG
  std::setenv("VK_LAYER_PATH",
              GetVulkanSdkPath("share/vulkan/explicit_layer.d").c_str(),
              /*overwrite=*/1);
#endif /* !NDEBUG */

#elif defined(__linux__)
#ifndef NDEBUG
  std::setenv("VK_LAYER_PATH",
              GetVulkanSdkPath("etc/vulkan/explicit_layer.d").c_str(),
              /*overwrite=*/1);
#endif /* !NDEBUG */

#endif /* __APPLE__ || __linux__ */

#endif /* USE_VULKAN */

  // We don't catch exceptions in the debug mode, so that if there is anything
  // wrong, the debugger would stay at the point where the program breaks.
#ifdef NDEBUG
  try {
#endif /* NDEBUG */
    ExampleType example{std::forward<ExampleArgs>(example_args)...};
    example.MainLoop();
#ifdef NDEBUG
  } catch (const std::exception& e) {
    LOG_ERROR << "Error: " << e.what();
    return EXIT_FAILURE;
  }
#endif /* NDEBUG */

  return EXIT_SUCCESS;
}

} /* namespace example */
} /* namespace lighter */

#endif /* LIGHTER_EXAMPLE_UTIL_H */