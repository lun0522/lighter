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
#include <stdexcept>
#include <type_traits>

#include "jessie_steamer/wrapper/vulkan/window_context.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {

inline void SetBuildEnvironment() {
  setenv("VK_ICD_FILENAMES",
         "external/lib-vulkan/etc/vulkan/icd.d/MoltenVK_icd.json", 1);
#ifndef NDEBUG
  setenv("VK_LAYER_PATH", "external/lib-vulkan/etc/vulkan/explicit_layer.d", 1);
#endif /* !NDEBUG */
}

class Application {
 public:
  Application() = default;

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
int AppMain() {
  static_assert(std::is_base_of<Application, AppType>::value,
                "Not a subclass of Application");

  SetBuildEnvironment();
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
