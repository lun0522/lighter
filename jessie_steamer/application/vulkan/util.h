//
//  util.h
//
//  Created by Pujun Lun on 6/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_UTIL_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_UTIL_H

#include <cstdlib>
#include <iostream>
#include <type_traits>

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/window_context.h"
#include "third_party/absl/flags/flag.h"

// Alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
#define ALIGN_MAT4 alignas(16)
#define ALIGN_VEC4 alignas(16)

ABSL_DECLARE_FLAG(bool, performance_mode);

namespace jessie_steamer {
namespace application {
namespace vulkan {

class Application {
 public:
  template <typename... Args>
  explicit Application(Args&&... args)
    : window_context_{std::forward<Args>(args)...} {}

  // This class is neither copyable nor movable.
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  virtual void MainLoop() = 0;

 protected:
  wrapper::vulkan::SharedBasicContext context() const {
    return window_context_.basic_context();
  }

  wrapper::vulkan::WindowContext window_context_;
};

template <typename AppType>
int AppMain(int argc, char* argv[]) {
  static_assert(std::is_base_of<Application, AppType>::value,
                "Not a subclass of Application");

  // Set up the path to find Vulkan SDK.
  common::util::ParseCommandLine(argc, argv);
  using common::file::GetVulkanSdkPath;

  if (absl::GetFlag(FLAGS_performance_mode)) {
    // To avoid the frame rate being clamped on MacOS when using MoltenVK:
    // https://github.com/KhronosGroup/MoltenVK/issues/581#issuecomment-487293665
    setenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS", "0", /*overwrite=*/1);
    setenv("MVK_CONFIG_PRESENT_WITH_COMMAND_BUFFER", "0", /*overwrite=*/1);
  }

  setenv("VK_ICD_FILENAMES",
         GetVulkanSdkPath("etc/vulkan/icd.d/MoltenVK_icd.json").c_str(),
         /*overwrite=*/1);
#ifndef NDEBUG
  setenv("VK_LAYER_PATH",
         GetVulkanSdkPath("etc/vulkan/explicit_layer.d").c_str(),
         /*overwrite=*/1);
#endif /* !NDEBUG */

  // We don't catch exceptions in the debug mode, so that if there is anything
  // wrong, the debugger would stay at the point it breaks.
#ifdef NDEBUG
  try {
#endif /* NDEBUG */
    AppType app{};
    app.MainLoop();
#ifdef NDEBUG
  } catch (const std::exception& e) {
    std::cerr << "Error: /n/t" << e.what() << std::endl;
    return EXIT_FAILURE;
  }
#endif /* NDEBUG */

  return EXIT_SUCCESS;
}

} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_UTIL_H */
