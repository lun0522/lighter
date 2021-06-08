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
#include <string_view>
#include <vector>

#include "lighter/common/graphics_api.h"
#include "lighter/common/window.h"
#include "lighter/renderer/renderer.h"
#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/renderer.h"
#include "third_party/absl/flags/declare.h"
#include "third_party/absl/flags/flag.h"

ABSL_DECLARE_FLAG(bool, ignore_vsync);

namespace lighter::renderer::util {

// Initializes the graphics API. This must be called once at the very beginning
// for each API that is going to be used.
void GlobalInit(common::api::GraphicsApi graphics_api);

// Creates a renderer that uses 'graphics_api' underneath.
std::unique_ptr<Renderer> CreateRenderer(
    common::api::GraphicsApi graphics_api, std::string_view application_name,
    const std::optional<debug_message::Config>& debug_message_config,
    std::vector<const common::Window*>&& window_ptrs);

}  // namespace lighter::renderer::util

#endif  // LIGHTER_RENDERER_UTIL_H
