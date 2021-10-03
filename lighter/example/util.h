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
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "lighter/common/data.h"
#include "lighter/common/file.h"
#include "lighter/common/graphics_api.h"
#include "lighter/common/timer.h"
#include "lighter/common/util.h"
#include "lighter/common/window.h"
#include "lighter/renderer/ir/buffer_util.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/pass.h"
#include "lighter/renderer/ir/pass_util.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/pipeline_util.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/util.h"
#include "third_party/absl/flags/parse.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"
#include "third_party/spirv_cross/spirv_cross.hpp"

namespace lighter::example {

using namespace renderer::ir;

inline std::unique_ptr<Renderer> CreateRenderer(
    common::api::GraphicsApi graphics_api, const char* application_name,
    std::vector<const common::Window*>&& window_ptrs) {
  std::optional<debug_message::Config> debug_message_config;
#ifndef NDEBUG
  using namespace debug_message;
  debug_message_config = {
      severity::WARNING | severity::ERROR,
      type::GENERAL | type::PERFORMANCE,
  };
#endif  // !NDEBUG
  return renderer::util::CreateRenderer(
      graphics_api, application_name, debug_message_config,
      std::move(window_ptrs));
}

// Returns the full path to compiled shader binary.
inline std::string GetShaderBinaryPath(std::string_view relative_path,
                                       common::api::GraphicsApi graphics_api) {
  return common::file::GetShaderBinaryPath(relative_path, graphics_api);
}

template <typename ExampleType, typename... ExampleArgs>
int ExampleMain(int argc, char* argv[], ExampleArgs&&... example_args) {
  absl::ParseCommandLine(argc, argv);
  common::file::EnableRunfileLookup(argv[0]);
  // TODO: Move to ctor of Example.
  renderer::util::GlobalInit(common::api::GraphicsApi::kVulkan);

  // We don't catch exceptions in the debug mode, so that if there is anything
  // wrong, the debugger would stay at the point where the program breaks.
#ifdef NDEBUG
  try {
#endif  // NDEBUG
    ExampleType example{std::forward<ExampleArgs>(example_args)...};
    example.MainLoop();
#ifdef NDEBUG
  } catch (const std::exception& e) {
    LOG_ERROR << "Error: " << e.what();
    return EXIT_FAILURE;
  }
#endif  // NDEBUG

  return EXIT_SUCCESS;
}

}  // namespace lighter::example

#endif  // LIGHTER_EXAMPLE_UTIL_H
