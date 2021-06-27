//
//  util.h
//
//  Created by Pujun Lun on 6/7/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_UTIL_H
#define LIGHTER_RENDERER_UTIL_H

#include <memory>
#include <optional>
#include <vector>

#include "lighter/common/graphics_api.h"
#include "lighter/common/window.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/renderer/ir/type.h"
#ifdef USE_VULKAN
#include "lighter/renderer/vk/renderer.h"
#endif  // USE_VULKAN
#include "third_party/absl/flags/declare.h"
#include "third_party/absl/flags/flag.h"

// We use the uniform block layout std140 in all shaders. The following
// alignment requirements must be enforced on the host code:
// https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_uniform_buffer_object.txt
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/chap14.html#interfaces-resources-layout
#define ALIGN_SCALAR(type) alignas(sizeof(type))
#define ALIGN_VEC4 alignas(sizeof(float) * 4)
#define ALIGN_MAT4 alignas(sizeof(float) * 4)

ABSL_DECLARE_FLAG(bool, ignore_vsync);

namespace lighter::renderer::util {

// Initializes the graphics API. This must be called once at the very beginning
// for each API that is going to be used.
void GlobalInit(common::api::GraphicsApi graphics_api);

// Creates a renderer that uses 'graphics_api' underneath.
std::unique_ptr<ir::Renderer> CreateRenderer(
    common::api::GraphicsApi graphics_api, const char* application_name,
    const std::optional<ir::debug_message::Config>& debug_message_config,
    std::vector<const common::Window*>&& window_ptrs);

}  // namespace lighter::renderer::util

#endif  // LIGHTER_RENDERER_UTIL_H
