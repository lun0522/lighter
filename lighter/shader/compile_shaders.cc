//
//  compile_shaders.cc
//
//  Created by Pujun Lun on 5/14/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <exception>
#include <filesystem>

#include "lighter/common/util.h"
#include "lighter/shader/compilation_record.h"
#include "third_party/absl/flags/flag.h"

ABSL_FLAG(std::string, shader_dir, "", "Path to the shader directory");

int main(int argc, char* argv[]) {
  namespace stdfs = std::filesystem;
  using namespace lighter::shader;

  absl::ParseCommandLine(argc, argv);

  try {
    const std::string shader_dir = absl::GetFlag(FLAGS_shader_dir);
    ASSERT_NON_EMPTY(shader_dir, "Please specify path to shader directory with --shader_dir");
    ASSERT_TRUE(stdfs::is_directory(stdfs::path{shader_dir}),
                "--shader_dir must be a valid directory");

    auto [reader, writer] =
        CompilationRecordHandler::CreateHandlers(shader_dir);
  } catch (const std::exception& e) {
    LOG_INFO << e.what();
  }

  return 0;
}
