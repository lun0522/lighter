//
//  compile_shaders.cc
//
//  Created by Pujun Lun on 5/14/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <exception>
#include <filesystem>
#include <optional>
#include <string_view>

#include "lighter/common/util.h"
#include "lighter/shader/compiler.h"
#include "lighter/shader/run_compiler.h"
#include "third_party/absl/flags/flag.h"

ABSL_FLAG(std::string, shader_dir, "", "Path to the shader directory");
ABSL_FLAG(std::string, opt_level, "perf",
          "Optimization level (none/size/perf)");

namespace {

using OptimizationLevel = lighter::shader::CompilerOptions::OptimizationLevel;

// Returns optimization level to use or std::nullopt if unrecognized.
std::optional<OptimizationLevel> ConvertOptimizationLevel(
    std::string_view opt_level) {
  if (opt_level == "none") {
    return OptimizationLevel::kNone;
  } else if (opt_level == "size") {
    return OptimizationLevel::kSize;
  } else if (opt_level == "perf") {
    return OptimizationLevel::kPerformance;
  }
  return std::nullopt;
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace stdfs = std::filesystem;

  try {
    absl::ParseCommandLine(argc, argv);

    const stdfs::path shader_dir{absl::GetFlag(FLAGS_shader_dir)};
    ASSERT_TRUE(stdfs::is_directory(shader_dir),
                "Please specify a valid shader directory with --shader_dir");

    const std::optional<OptimizationLevel> opt_level =
        ConvertOptimizationLevel(absl::GetFlag(FLAGS_opt_level));
    ASSERT_HAS_VALUE(opt_level,
                     "--opt_level must either be 'none', 'size' or 'perf'");

    lighter::shader::compiler::CompileShaders(shader_dir, opt_level.value());
  } catch (const std::exception& e) {
    LOG_INFO << e.what();
  }

  return 0;
}
