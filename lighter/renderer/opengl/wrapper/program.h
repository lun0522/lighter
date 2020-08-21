//
//  program.h
//
//  Created by Pujun Lun on 8/20/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_OPENGL_WRAPPER_PROGRAM_H
#define LIGHTER_RENDERER_OPENGL_WRAPPER_PROGRAM_H

#include "lighter/common/ref_count.h"
#include "third_party/glad/glad.h"

namespace lighter {
namespace renderer {
namespace opengl {

class Shader {
 public:
  using RefCountedShaderModule = common::RefCountedObject<Shader>;

  using AutoReleaseShaderPool = RefCountedShaderModule::AutoReleasePool;

  Shader(GLenum shader_type, const std::string& file_path);

  // This class is neither copyable nor movable.
  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;

  ~Shader() { glDeleteShader(shader_); }

 private:
  const GLuint shader_;
};

} /* namespace opengl */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_OPENGL_WRAPPER_PROGRAM_H */
