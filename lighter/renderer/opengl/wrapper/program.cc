//
//  program.cc
//
//  Created by Pujun Lun on 8/20/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/opengl/wrapper/program.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "lighter/common/data.h"
#include "lighter/common/file.h"
#include "lighter/common/util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace opengl {
namespace {

using ParameterGetter = void (*)(GLuint, GLenum, GLint*);

using InfoLogGetter = void (*)(GLuint, GLsizei, GLsizei*, GLchar*);

// Checks status using 'parameter_getter'. If there is any error, returns the
// error fetched with 'info_log_getter'. Otherwise, returns std::nullopt.
std::optional<std::vector<char>> CheckStatus(GLuint source, GLenum target,
                                             ParameterGetter parameter_getter,
                                             InfoLogGetter info_log_getter) {
  GLint success;
  parameter_getter(source, target, &success);
  if (success == GL_TRUE) {
    return std::nullopt;
  }

  GLint info_log_length;
  parameter_getter(source, GL_INFO_LOG_LENGTH, &info_log_length);

  std::vector<char> info_log(info_log_length);
  info_log_getter(source, info_log_length, /*length=*/nullptr, info_log.data());
  return info_log;
}

} /* namespace */

Shader::Shader(GLenum shader_type, const std::string& file_path)
    : shader_type_{shader_type}, shader_{glCreateShader(shader_type)} {
  const common::Data file_data = common::file::LoadDataFromFile(file_path);
  glShaderBinary(/*count=*/1, &shader_, GL_SHADER_BINARY_FORMAT_SPIR_V,
                 file_data.data<char>(), file_data.size());
  glSpecializeShader(shader_, "main", /*numSpecializationConstants=*/0,
                     /*pConstantIndex=*/nullptr, /*pConstantValue=*/nullptr);

  if (const auto error = CheckStatus(shader_, GL_COMPILE_STATUS, glGetShaderiv,
                                     glGetShaderInfoLog)) {
    FATAL(absl::StrFormat("Failed to compile shader loaded from '%s': %s",
                          file_path, error.value().data()));
  }
}

Program::Program(const absl::flat_hash_map<GLenum, std::string>&
                     shader_type_to_file_path_map)
    : program_{glCreateProgram()} {
  // Prevent shaders from being auto released.
  Shader::AutoReleaseShaderPool shader_pool;

  std::vector<GLuint> shaders;
  shaders.reserve(shader_type_to_file_path_map.size());

  for (const auto& pair : shader_type_to_file_path_map) {
    const GLenum shader_type = pair.first;
    const std::string& file_path = pair.second;
    const auto shader = Shader::RefCountedShader::Get(
        /*identifier=*/file_path, shader_type, file_path);
    ASSERT_TRUE(shader->shader_type() == shader_type,
                absl::StrFormat("Previous shader type specified for '%s' was "
                                "%d, but now type %d instead",
                                file_path, shader->shader_type(), shader_type));

    glAttachShader(program_, **shader);
    shaders.push_back(**shader);
  }

  glLinkProgram(program_);
  if (const auto error = CheckStatus(program_, GL_LINK_STATUS, glGetProgramiv,
                                     glGetProgramInfoLog)) {
    FATAL(absl::StrFormat("Failed to link program: %s", error.value().data()));
  }
  for (const GLuint shader : shaders) {
    glDetachShader(program_, shader);
  }
}

void Program::BindUniformBuffer(const std::string& uniform_block_name,
                                GLuint binding_point) const {
  const GLint uniform_block_index =
      glGetUniformBlockIndex(program_, uniform_block_name.data());
  glUniformBlockBinding(program_, uniform_block_index, binding_point);
}

} /* namespace opengl */
} /* namespace renderer */
} /* namespace lighter */
