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

#include "lighter/common/window.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/vk/renderer.h"
#include <absl/flags/declare.h>
#include <absl/flags/flag.h>

// We use the uniform block layout std140 in all shaders. The following
// alignment requirements must be enforced on the host code:
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/chap14.html#interfaces-resources-layout
#define ALIGN_SCALAR(type) alignas(sizeof(type))
#define ALIGN_VEC4 alignas(sizeof(float) * 4)
#define ALIGN_MAT4 alignas(sizeof(float) * 4)

ABSL_DECLARE_FLAG(bool, ignore_vsync);

namespace lighter::renderer::util {

// Initializes the graphics API. This must be called once at the very beginning.
void GlobalInit();

// Creates a Vulkan-backed renderer.
std::unique_ptr<ir::Renderer> CreateRenderer(
    const char* application_name,
    const std::optional<ir::debug_message::Config>& debug_message_config,
    std::vector<const common::Window*>&& window_ptrs);

}  // namespace lighter::renderer::util

#endif  // LIGHTER_RENDERER_UTIL_H
