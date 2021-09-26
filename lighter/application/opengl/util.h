//
//  util.h
//
//  Created by Pujun Lun on 8/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_APPLICATION_OPENGL_UTIL_H
#define LIGHTER_APPLICATION_OPENGL_UTIL_H

#include <string>

#include "lighter/common/file.h"
#include "lighter/common/graphics_api.h"
#include "lighter/common/timer.h"
#include "lighter/common/util.h"
#include "lighter/common/window.h"
#include "lighter/renderer/opengl/wrapper/program.h"
#include "lighter/renderer/util.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/flags/declare.h"
#include "third_party/absl/flags/flag.h"
#include "third_party/absl/flags/parse.h"
#include "third_party/glad/glad.h"
#include "third_party/glm/glm.hpp"
#include "third_party/GLFW/glfw3.h"

namespace lighter {
namespace application {
namespace opengl {

// This is the base class of all applications. It simply owns a window. Each
// application should overwrite MainLoop() to render custom scenes.
class Application {
 public:
  // This class is neither copyable nor movable.
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  ~Application() = default;

  // Main loop of the application.
  virtual void MainLoop() = 0;

 protected:
  Application(const std::string& name, const glm::ivec2& screen_size)
      : window_{name, screen_size} {}

  // Accessors.
  common::Window* mutable_window() { return &window_; }
  const common::Window& window() const { return window_; }

 private:
  // Wrapper of GLFWwindow.
  common::Window window_;
};

// Parses command line arguments, instantiates an application of AppType, and
// runs its MainLoop(). AppType must be a subclass of Application.
template <typename AppType>
int AppMain(int argc, char* argv[]) {
  static_assert(std::is_base_of_v<Application, AppType>,
                "Not a subclass of Application");

  absl::ParseCommandLine(argc, argv);
  common::file::EnableRunfileLookup(argv[0]);

  // We don't catch exceptions in the debug mode, so that if there is anything
  // wrong, the debugger would stay at the point where the application breaks.
#ifdef NDEBUG
  try {
#endif /* NDEBUG */
  AppType app;
  if (absl::GetFlag(FLAGS_ignore_vsync)) {
    glfwSwapInterval(0);
  }
  app.MainLoop();
#ifdef NDEBUG
  } catch (const std::exception& e) {
    LOG_ERROR << "Error: " << e.what();
    return EXIT_FAILURE;
  }
#endif /* NDEBUG */

  return EXIT_SUCCESS;
}

} /* namespace opengl */
} /* namespace application */
} /* namespace lighter */

#endif /* LIGHTER_APPLICATION_OPENGL_UTIL_H */
