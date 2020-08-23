//
//  program.h
//
//  Created by Pujun Lun on 8/20/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_OPENGL_WRAPPER_PROGRAM_H
#define LIGHTER_RENDERER_OPENGL_WRAPPER_PROGRAM_H

#include <string>

#include "lighter/common/ref_count.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/glad/glad.h"

namespace lighter {
namespace renderer {
namespace opengl {

// TODO
class Shader {
 public:
  using RefCountedShader = common::RefCountedObject<Shader>;

  using AutoReleaseShaderPool = RefCountedShader::AutoReleasePool;

  Shader(GLenum shader_type, const std::string& file_path);

  // This class is neither copyable nor movable.
  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;

  ~Shader() { glDeleteShader(shader_); }

  // Overloads.
  GLuint operator*() const { return shader_; }

  // Accessors.
  GLenum shader_type() const { return shader_type_; }

 private:
  const GLenum shader_type_;

  const GLuint shader_;
};

class Program {
 public:
  explicit Program(const absl::flat_hash_map<GLenum, std::string>&
                       shader_type_to_file_path_map);

  // This class is neither copyable nor movable.
  Program(const Program&) = delete;
  Program& operator=(const Program&) = delete;

  ~Program() { glDeleteProgram(program_); }

  void Use() const { glUseProgram(program_); }

  // TODO
  void BindUniformBuffer(const std::string& uniform_block_name,
                         GLuint binding_point) const {
    const GLint uniform_block_index =
        glGetUniformBlockIndex(program_, uniform_block_name.data());
    glUniformBlockBinding(program_, uniform_block_index, binding_point);
  }

 private:
  const GLuint program_;
};

} /* namespace opengl */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_OPENGL_WRAPPER_PROGRAM_H */
