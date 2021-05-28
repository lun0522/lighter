//
//  compiler.h
//
//  Created by Pujun Lun on 5/25/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_SHADER_COMPILER_H
#define LIGHTER_SHADER_COMPILER_H

#include <optional>
#include <string>
#include <string_view>

#include "lighter/common/util.h"
#include "lighter/shader_compiler/util.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/span.h"
#include "third_party/shaderc/shaderc.h"

namespace lighter::shader_compiler {

// Forward declarations.
class CompilationResult;
class CompilerOptions;

// Wraps shaderc_compiler.
class Compiler {
 public:
  explicit Compiler()
      : compiler_{FATAL_IF_NULL(shaderc_compiler_initialize())} {}

  // This class is neither copyable nor movable.
  Compiler(const Compiler&) = delete;
  Compiler& operator=(const Compiler&) = delete;

  ~Compiler() { shaderc_compiler_release(compiler_); }

  // Returns shader kind if `file_extension` is recognized and supported, or
  // std::nullopt otherwise.
  static std::optional<shaderc_shader_kind> GetShaderKind(
      std::string_view file_extension);

  // Compiles a shader. 'shader_tag' is only used for cases like emitting error
  // messages, and it does not need to be a unique identifier.
  std::unique_ptr<CompilationResult> Compile(
      const std::string& shader_tag,
      shaderc_shader_kind shader_kind,
      absl::Span<const char> shader_source,
      const CompilerOptions& compiler_options) const;

 private:
  // Maps file extensions to shader kinds.
  using ShaderKindMap = absl::flat_hash_map<std::string, shaderc_shader_kind>;
  static const ShaderKindMap shader_kind_map_;

  // Opaque compiler object.
  shaderc_compiler_t compiler_;
};

// Wraps shaderc_compile_options.
class CompilerOptions {
 public:
  explicit CompilerOptions()
      : options_{FATAL_IF_NULL(shaderc_compile_options_initialize())} {}

  // This class is neither copyable nor movable.
  CompilerOptions(const CompilerOptions&) = delete;
  CompilerOptions& operator=(const CompilerOptions&) = delete;

  ~CompilerOptions() { shaderc_compile_options_release(options_); }

  // Sets the optimization level (none/size/performance).
  CompilerOptions& SetOptimizationLevel(OptimizationLevel level);

  // Adds a macro definition "-Dkey=value" if 'value' has value, or just "-Dkey"
  // otherwise. If this is called multiple times with the same 'key', the last
  // one will overwrite the previous ones.
  CompilerOptions& AddMacroDefinition(
      const std::string& key,
      const std::optional<std::string>& value = std::nullopt);

  // Overloads.
  const shaderc_compile_options_t& operator*() const { return options_; }

 private:
  // Opaque compiler options object.
  shaderc_compile_options_t options_;
};

// Wraps shaderc_compilation_result.
class CompilationResult {
 public:
  explicit CompilationResult(shaderc_compilation_result_t result)
      : result_{FATAL_IF_NULL(result)} {}

  // This class is neither copyable nor movable.
  CompilationResult(const CompilationResult&) = delete;
  CompilationResult& operator=(const CompilationResult&) = delete;

  ~CompilationResult() { shaderc_result_release(result_); }

  // Returns a pointer to the error message if compilation failed, or nullptr
  // otherwise.
  const char* GetErrorIfFailed() const;

  // Returns the whole data span, which lives as long as the result object.
  absl::Span<const char> GetDataSpan() const {
    return {shaderc_result_get_bytes(result_),
            shaderc_result_get_length(result_)};
  }

 private:
  // Opaque compilation result object.
  shaderc_compilation_result_t result_;
};

}  // namespace lighter::shader_compiler

#endif  // LIGHTER_SHADER_COMPILER_H
