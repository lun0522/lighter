//
//  triangle.cc
//
//  Created by Pujun Lun on 8/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <memory>

#include "lighter/application/opengl/util.h"

namespace lighter {
namespace application {
namespace opengl {
namespace {

using namespace renderer::opengl;

constexpr uint32_t kUniformBufferBindingPoint = 0;

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct Alpha {
  ALIGN_SCALAR(float) float value;
};

/* END: Consistent with uniform blocks defined in shaders. */

} /* namespace */

class TriangleApp : public Application {
 public:
  explicit TriangleApp();

  // This class is neither copyable nor movable.
  TriangleApp(const TriangleApp&) = delete;
  TriangleApp& operator=(const TriangleApp&) = delete;

  // Overrides.
  void MainLoop() override;

 private:
  // Updates per-frame data.
  void UpdateData();

  common::FrameTimer timer_;
  std::unique_ptr<Program> program_;
  GLuint vertex_attributes_;
  GLuint vertex_buffer_;
  GLuint uniform_buffer_;
};

TriangleApp::TriangleApp()
    : Application{"Hello Triangle", /*screen_size=*/{800, 600}} {
  using common::Vertex3DWithColor;

  ASSERT_TRUE(gladLoadGL() != 0, "Failed to Load OpenGL");

  const glm::ivec2 frame_size = window().GetFrameSize();
  glViewport(/*x=*/0, /*y=*/0, /*width=*/frame_size.x, /*height=*/frame_size.y);

  program_ = absl::make_unique<Program>(
      absl::flat_hash_map<GLenum, std::string>{
          {GL_VERTEX_SHADER,
              common::file::GetGlShaderPath("triangle/triangle.vert")},
          {GL_FRAGMENT_SHADER,
              common::file::GetGlShaderPath("triangle/triangle.frag")}});

  const std::array<Vertex3DWithColor, 3> vertex_data{
      Vertex3DWithColor{/*pos=*/{0.5f, -0.5f, 0.0f},
                        /*color=*/{1.0f, 0.0f, 0.0f}},
      Vertex3DWithColor{/*pos=*/{0.0f, 0.5f, 0.0f},
                        /*color=*/{0.0f, 0.0f, 1.0f}},
      Vertex3DWithColor{/*pos=*/{-0.5f, -0.5f, 0.0f},
                        /*color=*/{0.0f, 1.0f, 0.0f}},
  };

  glGenVertexArrays(/*n=*/1, &vertex_attributes_);
  glGenBuffers(/*n=*/1, &vertex_buffer_);
  glGenBuffers(/*n=*/1, &uniform_buffer_);

  glBindVertexArray(vertex_attributes_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer_);

  glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex3DWithColor) * vertex_data.size(),
               vertex_data.data(), GL_STATIC_DRAW);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(Alpha), /*data=*/nullptr,
               GL_DYNAMIC_DRAW);

  // TODO
  program_->BindUniformBuffer("alpha", kUniformBufferBindingPoint);
  glBindBufferBase(GL_UNIFORM_BUFFER, kUniformBufferBindingPoint,
                   uniform_buffer_);

  glVertexAttribPointer(/*index=*/0, /*size=*/3, GL_FLOAT, GL_FALSE,
                        /*stride=*/sizeof(Vertex3DWithColor),
                        (void *)offsetof(Vertex3DWithColor, pos));
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(/*index=*/1, /*size=*/3, GL_FLOAT, GL_FALSE,
                        /*stride=*/sizeof(Vertex3DWithColor),
                        (void *)offsetof(Vertex3DWithColor, color));
  glEnableVertexAttribArray(1);

  // Unbind objects.
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void TriangleApp::UpdateData() {
  const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();
  glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer_);
  const float alpha = glm::abs(glm::sin(elapsed_time));
  glBufferData(GL_UNIFORM_BUFFER, sizeof(Alpha), /*data=*/&alpha,
               GL_DYNAMIC_DRAW);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void TriangleApp::MainLoop() {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  while (!window().ShouldQuit()) {
    timer_.Tick();
    UpdateData();

    glClear(GL_COLOR_BUFFER_BIT);

    program_->Use();
    glBindVertexArray(vertex_attributes_);
    glDrawArrays(GL_TRIANGLES, /*first=*/0, /*count=*/3);

    window().SwapFramebuffers();
    window().ProcessUserInputs();

    if (window().is_resized()) {
      const glm::ivec2 frame_size = mutable_window()->Recreate();
      glViewport(/*x=*/0, /*y=*/0, /*width=*/frame_size.x,
                 /*height=*/frame_size.y);
    }
  }
}

} /* namespace opengl */
} /* namespace application */
} /* namespace lighter */

int main(int argc, char* argv[]) {
  using namespace lighter::application::opengl;
  return AppMain<TriangleApp>(argc, argv);
}
