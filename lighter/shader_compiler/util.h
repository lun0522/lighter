//
//  util.h
//
//  Created by Pujun Lun on 5/26/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_SHADER_UTIL_H
#define LIGHTER_SHADER_UTIL_H

#include <filesystem>
#include <optional>
#include <string_view>

#include "lighter/common/graphics_api.h"

namespace lighter::shader_compiler {

constexpr const char kShaderEntryPoint[] = "main";

// Corresponds to text 'none', 'size' and 'perf'.
enum class OptimizationLevel { kNone, kSize, kPerformance };

namespace util {

// Converts the optimization level to text.
const char* OptLevelToText(OptimizationLevel level);

// Returns the optimization level if the text can be recognized.
// Otherwise, returns std::nullopt.
std::optional<OptimizationLevel> OptLevelFromText(std::string_view text);

// Returns the path to compiled shader binary relative to the shader directory.
// 'relative_path' refers to the path to source shader file.
std::filesystem::path GetShaderBinaryPath(
    common::api::GraphicsApi graphics_api,
    const std::filesystem::path& relative_path);

}  // namespace util
}  // namespace lighter::shader_compiler

#endif  // LIGHTER_SHADER_UTIL_H
