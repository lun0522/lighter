//
//  util.cc
//
//  Created by Pujun Lun on 5/26/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/shader_compiler/util.h"

namespace lighter::shader_compiler::util {
namespace {

namespace stdfs = std::filesystem;

constexpr const char kSpirvBinaryFileExtension[] = ".spv";
constexpr const char kOptLevelNoneText[] = "none";
constexpr const char kOptLevelSizeText[] = "size";
constexpr const char kOptLevelPerfText[] = "perf";

}  // namespace

const char* OptLevelToText(OptimizationLevel level) {
  switch (level) {
    case OptimizationLevel::kNone:
      return kOptLevelNoneText;
    case OptimizationLevel::kSize:
      return kOptLevelSizeText;
    case OptimizationLevel::kPerformance:
      return kOptLevelPerfText;
  }
}

std::optional<OptimizationLevel> OptLevelFromText(std::string_view text) {
  if (text == kOptLevelNoneText) {
    return OptimizationLevel::kNone;
  } else if (text == kOptLevelSizeText) {
    return OptimizationLevel::kSize;
  } else if (text == kOptLevelPerfText) {
    return OptimizationLevel::kPerformance;
  }
  return std::nullopt;
}

stdfs::path GetShaderBinaryPath(common::api::GraphicsApi graphics_api,
                                const stdfs::path& relative_path) {
  stdfs::path res{common::api::GetApiAbbreviatedName(graphics_api)};
  res /= relative_path;
  res += kSpirvBinaryFileExtension;
  return res;
}

}  // namespace lighter::shader_compiler::util
