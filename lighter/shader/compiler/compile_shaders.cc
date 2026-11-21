//
//  compile_shaders.cc
//
//  Created by Pujun Lun on 5/14/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <exception>
#include <filesystem>
#include <optional>

#include "lighter/base/flag.h"
#include "lighter/base/util.h"
#include "lighter/shader/compiler/run_compiler.h"
#include "lighter/shader/compiler/util.h"

LIGHTER_DECLARE_FLAG(std::string, shader_dir);
LIGHTER_FLAG(std::string, opt_level, "perf",
             "Optimization level (none/size/perf)");

int main(int argc, char* argv[]) {
  namespace stdfs = std::filesystem;
  namespace sc = lighter::shader::compiler;

  try {
    lighter::base::flag::ParseCommandLine(argc, argv);

    stdfs::path shader_dir{FLAGS_shader_dir.Get()};
    ASSERT_TRUE(stdfs::is_directory(shader_dir),
                "Please specify a valid shader directory with --shader_dir");

    const std::optional<sc::OptimizationLevel> opt_level =
        sc::util::OptLevelFromText(FLAGS_opt_level.Get());
    ASSERT_HAS_VALUE(opt_level,
                     "--opt_level must either be 'none', 'size' or 'perf'");

    sc::compiler::CompileShaders(std::move(shader_dir), opt_level.value());
  } catch (const std::exception& e) {
    LOG_INFO << e.what();
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
