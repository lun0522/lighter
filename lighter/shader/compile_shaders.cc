//
//  compile_shaders.cc
//
//  Created by Pujun Lun on 5/14/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <exception>
#include <filesystem>

#include "lighter/common/util.h"
#include "lighter/shader/compiler.h"
#include "third_party/absl/flags/flag.h"

ABSL_FLAG(std::string, shader_dir, "", "Path to the shader directory");

int main(int argc, char* argv[]) {
  namespace stdfs = std::filesystem;

  try {
    absl::ParseCommandLine(argc, argv);
    const stdfs::path shader_dir{absl::GetFlag(FLAGS_shader_dir)};
    ASSERT_TRUE(stdfs::is_directory(shader_dir),
                "Please specify a valid shader directory with --shader_dir");
    lighter::shader::compiler::Compile(shader_dir);
  } catch (const std::exception& e) {
    LOG_INFO << e.what();
  }

  return 0;
}
