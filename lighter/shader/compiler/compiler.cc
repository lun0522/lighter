#include "lighter/shader/compiler/compiler.h"

#include <format>

namespace lighter::shader::compiler {
namespace {

constexpr const char kShaderEntryPoint[] = "main";
constexpr const char kOptLevelNoneText[] = "none";
constexpr const char kOptLevelSizeText[] = "size";
constexpr const char kOptLevelPerfText[] = "perf";

// Converts optimization level enums.
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

const Compiler::ShaderKindMap Compiler::shader_kind_map_ = {
    {".vert", shaderc_vertex_shader},
    {".frag", shaderc_fragment_shader},
    {".comp", shaderc_compute_shader},
};

std::optional<shaderc_shader_kind> Compiler::GetShaderKind(
    std::string_view file_extension) {
  if (const auto iter = shader_kind_map_.find(file_extension);
      iter != shader_kind_map_.end()) {
    return iter->second;
  }
  return std::nullopt;
}

std::unique_ptr<CompilationResult> Compiler::Compile(
    const std::string& shader_tag, shaderc_shader_kind shader_kind,
    std::span<const char> shader_source,
    const CompilerOptions& compiler_options) const {
  auto result = std::make_unique<CompilationResult>(shaderc_compile_into_spv(
      compiler_, shader_source.data(), shader_source.size(), shader_kind,
      shader_tag.data(), kShaderEntryPoint, *compiler_options));
  if (const char* error_message = result->GetErrorIfFailed()) {
    FATAL(std::format("Failed to compile {}: {}", shader_tag, error_message));
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
  if (shaderc_result_get_compilation_status(result_) !=
      shaderc_compilation_status_success) {
    return shaderc_result_get_error_message(result_);
  }
  return nullptr;
}

}  // namespace lighter::shader::compiler
