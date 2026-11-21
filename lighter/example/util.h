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
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <spirv_cross/spirv_cross.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "lighter/common/camera.h"
#include "lighter/common/data.h"
#include "lighter/common/file.h"
#include "lighter/common/flag.h"
#include "lighter/common/timer.h"
#include "lighter/common/util.h"
#include "lighter/common/window.h"
#include "lighter/renderer/ir/buffer_util.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/pipeline_util.h"
#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/util.h"

namespace lighter::example {

inline std::unique_ptr<renderer::ir::Renderer> CreateRenderer(
    const char* application_name, std::vector<common::Window*>&& window_ptrs) {
  std::optional<renderer::ir::debug_message::Config> debug_message_config;
#ifndef NDEBUG
  namespace sev = renderer::ir::debug_message::severity;
  namespace type = renderer::ir::debug_message::type;
  debug_message_config = {
      sev::WARNING | sev::ERROR,
      type::GENERAL | type::PERFORMANCE,
  };
#endif  // !NDEBUG
  return renderer::util::CreateRenderer(application_name, debug_message_config,
                                        std::move(window_ptrs));
}

// Returns the full path to compiled shader binary.
inline std::string GetShaderBinaryPath(std::string_view relative_path) {
  return common::file::GetShaderBinaryPath(relative_path);
}

// Configures color (with optional MSAA resolve) and optional depth-stencil
// attachments for the main onscreen graphics pass.
inline void SetupMainPassAttachments(
    renderer::ir::PassBuilder& builder, renderer::ir::ImageHandle backbuffer,
    renderer::ir::MultisamplingMode multisampling_mode, bool use_depth) {
  using renderer::ir::AttachmentLoadOp;
  using renderer::ir::AttachmentStoreOp;
  using renderer::ir::ImageHandle;
  using renderer::ir::MultisamplingMode;
  using renderer::ir::TransientImageDesc;
  using renderer::ir::TransientImageFormat;

  if (multisampling_mode != MultisamplingMode::kNone) {
    ImageHandle msaa = builder.CreateImage(TransientImageDesc{
        .name = "color_multisample",
        .format = TransientImageFormat::kMatchExternalImage,
        .multisampling_mode = multisampling_mode,
        .match_image = backbuffer,
    });
    msaa =
        builder.ColorAttachment(msaa, /*location=*/0, AttachmentLoadOp::kClear,
                                AttachmentStoreOp::kDontCare);
    builder.ResolveAttachment(msaa, backbuffer);
  } else {
    builder.ColorAttachment(backbuffer, /*location=*/0);
  }

  if (use_depth) {
    const ImageHandle depth = builder.CreateImage(TransientImageDesc{
        .name = "depth_stencil",
        .format = TransientImageFormat::kDepthStencil,
        .multisampling_mode = multisampling_mode,
        .match_image = backbuffer,
    });
    builder.DepthAttachment(depth, AttachmentLoadOp::kClear,
                            AttachmentStoreOp::kDontCare);
  }
}

// Binds standard mouse-look, scroll-zoom, arrow-key movement, and
// Escape-to-quit controls to 'window' for 'camera'.
inline void RegisterCameraControls(
    common::Window& window, const common::FrameTimer& timer,
    common::UserControlledPerspectiveCamera& camera, bool* should_quit) {
  using WindowKey = common::Window::KeyMap;
  using ControlKey = common::camera_control::Key;

  window.SetCursorHidden(true)
      .RegisterMoveCursorCallback([&camera](double x_pos, double y_pos) {
        camera.DidMoveCursor(x_pos, y_pos);
      })
      .RegisterScrollCallback([&camera](double /*x_pos*/, double y_pos) {
        camera.DidScroll(y_pos, 1.0f, 60.0f);
      })
      .RegisterPressKeyCallback(WindowKey::kUp,
                                [&camera, &timer]() {
                                  camera.DidPressKey(
                                      ControlKey::kUp,
                                      timer.GetElapsedTimeSinceLastFrame());
                                })
      .RegisterPressKeyCallback(WindowKey::kDown,
                                [&camera, &timer]() {
                                  camera.DidPressKey(
                                      ControlKey::kDown,
                                      timer.GetElapsedTimeSinceLastFrame());
                                })
      .RegisterPressKeyCallback(WindowKey::kLeft,
                                [&camera, &timer]() {
                                  camera.DidPressKey(
                                      ControlKey::kLeft,
                                      timer.GetElapsedTimeSinceLastFrame());
                                })
      .RegisterPressKeyCallback(WindowKey::kRight,
                                [&camera, &timer]() {
                                  camera.DidPressKey(
                                      ControlKey::kRight,
                                      timer.GetElapsedTimeSinceLastFrame());
                                })
      .RegisterPressKeyCallback(WindowKey::kEscape,
                                [should_quit]() { *should_quit = true; });
}

template <typename ExampleType, typename... ExampleArgs>
int ExampleMain(int argc, char* argv[], ExampleArgs&&... example_args) {
  common::flag::ParseCommandLine(argc, argv);
  // TODO: Move to ctor of Example.
  renderer::util::GlobalInit();

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
