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

// This class loads a shader from 'file_path', compiles it and holds the
// resulting shader handle. Shaders can be released after the program is linked
// in order to save the host memory. The user can avoid this happening by
// instantiating an AutoReleaseShaderPool.
class Shader {
 public:
  // Reference counted shaders.
  using RefCountedShader = common::RefCountedObject<Shader>;

  // An instance of this will preserve all shaders created within its
  // surrounding scope, and release them once all AutoReleaseShaderPool objects
  // go out of scope.
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
  // Type of shader.
  const GLenum shader_type_;

  // Shader handle.
  const GLuint shader_;
};

// This class creates a program handle, and links shaders to it.
class Program {
 public:
  explicit Program(const absl::flat_hash_map<GLenum, std::string>&
                       shader_type_to_file_path_map);

  // This class is neither copyable nor movable.
  Program(const Program&) = delete;
  Program& operator=(const Program&) = delete;

  ~Program() { glDeleteProgram(program_); }

  // Binds the uniform buffer block to 'binding_point'.
  void BindUniformBuffer(const std::string& uniform_block_name,
                         GLuint binding_point) const;

  // Binds this program.
  void Use() const { glUseProgram(program_); }

 private:
  // Program handle.
  const GLuint program_;
};

} /* namespace opengl */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_OPENGL_WRAPPER_PROGRAM_H */
