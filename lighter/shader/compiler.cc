//
//  compiler.cc
//
//  Created by Pujun Lun on 5/25/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/shader/compiler.h"

#include "lighter/common/graphics_api.h"

namespace lighter::shader {
namespace {

using OptimizationLevel = CompilerOptions::OptimizationLevel;

shaderc_optimization_level GetOptimizationLevelFlag(OptimizationLevel level) {
  switch (level) {
    case OptimizationLevel::kNone:
      return shaderc_optimization_level_zero;
    case OptimizationLevel::kSize:
      return shaderc_optimization_level_size;
    case OptimizationLevel::kPerformance:
      return shaderc_optimization_level_performance;
  }
}

}  // namespace

const Compiler::ShaderKindMap Compiler::shader_kind_map_ = {
    {".vert", shaderc_glsl_default_vertex_shader},
    {".frag", shaderc_glsl_default_fragment_shader},
    {".comp", shaderc_glsl_default_compute_shader},
};

std::optional<shaderc_shader_kind> Compiler::GetShaderKind(
    std::string_view file_extension) {
  const auto iter = shader_kind_map_.find(file_extension);
  return iter != shader_kind_map_.end() ? std::make_optional(iter->second)
                                        : std::nullopt;
}

std::unique_ptr<CompilationResult> Compiler::Compile(
    const std::string& shader_tag,
    shaderc_shader_kind shader_kind,
    absl::Span<const char> shader_source,
    const CompilerOptions& compiler_options) const {
  auto result = std::make_unique<CompilationResult>(
      shaderc_compile_into_spv_assembly(
          compiler_, shader_source.data(), shader_source.size(), shader_kind,
          shader_tag.data(), common::api::kShaderEntryPoint, *compiler_options)
  );
  if (const char* error_message = result->GetErrorIfFailed()) {
    FATAL(absl::StrFormat("Failed to compile %s: %s",
                          shader_tag, error_message));
  }
  return result;
}

CompilerOptions& CompilerOptions::SetOptimizationLevel(
    OptimizationLevel level) {
  shaderc_compile_options_set_optimization_level(
      options_, GetOptimizationLevelFlag(level));
  return *this;
}

CompilerOptions& CompilerOptions::AddMacroDefinition(
    const std::string& key, const std::optional<std::string>& value) {
  shaderc_compile_options_add_macro_definition(
      options_, key.data(), key.length(),
      value.has_value() ? value->data() : nullptr,
      value.has_value() ? value->length() : 0);
  return *this;
}

const char* CompilationResult::GetErrorIfFailed() const {
  const bool succeeded = shaderc_result_get_compilation_status(result_) ==
                         shaderc_compilation_status_success;
  return succeeded ? nullptr : shaderc_result_get_error_message(result_);
}

}  // namespace lighter::shader
