//
//  program.cc
//
//  Created by Pujun Lun on 8/20/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/opengl/wrapper/program.h"

#include <vector>

#include "lighter/common/file.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace opengl {

Shader::Shader(GLenum shader_type, const std::string& file_path)
    : shader_{glCreateShader(shader_type)} {
  const auto raw_data = absl::make_unique<common::RawData>(file_path);
  glShaderBinary(/*count=*/1, &shader_, GL_SHADER_BINARY_FORMAT_SPIR_V,
                 raw_data->data, raw_data->size);
  glSpecializeShader(shader_, "main", /*numSpecializationConstants=*/0,
                     /*pConstantIndex=*/nullptr, /*pConstantValue=*/nullptr);

  GLint success;
  glGetShaderiv(shader_, GL_COMPILE_STATUS, &success);
  if (success == GL_FALSE) {
    GLint info_log_length;
    glGetShaderiv(shader_, GL_INFO_LOG_LENGTH, &info_log_length);

    std::vector<char> info_log(info_log_length);
    glGetShaderInfoLog(shader_, info_log_length, /*length=*/nullptr,
                       info_log.data());
    FATAL(absl::StrFormat("Failed to compile shader loaded from %s: %s",
                          file_path, info_log.data()));
  }
}

} /* namespace opengl */
} /* namespace renderer */
} /* namespace lighter */
