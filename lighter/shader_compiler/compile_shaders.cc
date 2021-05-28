//
//  compile_shaders.cc
//
//  Created by Pujun Lun on 5/14/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <exception>
#include <filesystem>
#include <optional>

#include "lighter/common/util.h"
#include "lighter/shader_compiler/run_compiler.h"
#include "lighter/shader_compiler/util.h"
#include "third_party/absl/flags/flag.h"

ABSL_FLAG(std::string, shader_dir, "", "Path to the shader directory");
ABSL_FLAG(std::string, opt_level, "perf",
          "Optimization level (none/size/perf)");

int main(int argc, char* argv[]) {
  namespace stdfs = std::filesystem;
  using namespace lighter::shader_compiler;

  try {
    lighter::common::util::ParseCommandLine(argc, argv);

    stdfs::path shader_dir{absl::GetFlag(FLAGS_shader_dir)};
    ASSERT_TRUE(stdfs::is_directory(shader_dir),
                "Please specify a valid shader directory with --shader_dir");

    const std::optional<OptimizationLevel> opt_level =
        util::OptLevelFromText(absl::GetFlag(FLAGS_opt_level));
    ASSERT_HAS_VALUE(opt_level,
                     "--opt_level must either be 'none', 'size' or 'perf'");

    compiler::CompileShaders(std::move(shader_dir), opt_level.value());
  } catch (const std::exception& e) {
    LOG_INFO << e.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
