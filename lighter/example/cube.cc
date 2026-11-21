//
//  cube.cc
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <string>

#include "lighter/example/util.h"
#include "lighter/renderer/extension/model.h"
#include "lighter/renderer/extension/text.h"

namespace lighter::example {
namespace {

using renderer::extension::DynamicText;
using renderer::extension::Model;
using renderer::extension::ModelBuilder;
using renderer::extension::StaticText;
using renderer::extension::Text;
using renderer::ir::ImageHandle;
using renderer::ir::MultisamplingMode;
using renderer::ir::PassBuilder;
using renderer::ir::PassContext;
using renderer::ir::Renderer;
using renderer::ir::RenderGraph;

constexpr int kWindowIndex = 0;
constexpr int kObjFileIndexBase = 1;

// Consistent with uniform blocks defined in shaders.
struct Transformation {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

class CubeExample {
 public:
  CubeExample(const glm::ivec2& screen_size,
              MultisamplingMode multisampling_mode)
      : multisampling_mode_{multisampling_mode}, window_{"Cube", screen_size} {
    renderer_ = CreateRenderer("Cube Example", {&window_});

    const float aspect_ratio = window_.original_aspect_ratio();
    cube_model_ =
        ModelBuilder{
            *renderer_, "Cube", aspect_ratio,
            ModelBuilder::SingleMeshResource{
                common::file::GetResourcePath("model/cube.obj"),
                kObjFileIndexBase,
                /*tex_source_map=*/
                {{
                    ModelBuilder::TextureType::kDiffuse,
                    {common::file::GetResourcePath("texture/statue.jpg")},
                }},
            }}
            .AddTextureBindingPoint(ModelBuilder::TextureType::kDiffuse,
                                    /*binding=*/1)
            .SetShader(renderer::ir::shader_stage::VERTEX,
                       GetShaderBinaryPath("cube/cube.vert"))
            .SetShader(renderer::ir::shader_stage::FRAGMENT,
                       GetShaderBinaryPath("cube/cube.frag"))
            .Build();

    constexpr auto kFont = Text::Font::kGeorgia;
    constexpr int kFontHeight = 100;
    const std::array<std::string, 1> static_texts{"FPS: "};
    const std::array<std::string, 1> dynamic_char_sets{"01234567890"};
    static_text_ = std::make_unique<StaticText>(
        *renderer_, aspect_ratio, static_texts, kFont, kFontHeight);
    dynamic_text_ = std::make_unique<DynamicText>(
        *renderer_, aspect_ratio, dynamic_char_sets, kFont, kFontHeight);
  }

  // This class is neither copyable nor movable.
  CubeExample(const CubeExample&) = delete;
  CubeExample& operator=(const CubeExample&) = delete;

  void MainLoop() {
    constexpr float kTextHeight = 0.05f;
    constexpr float kTextBaseX = 0.04f;
    constexpr float kTextBaseY = 0.05f;
    constexpr float kTextAlpha = 0.5f;
    const glm::vec3 text_color{1.0f};

    RenderGraph graph;
    while (!window_.ShouldQuit()) {
      window_.ProcessUserInputs();
      timer_.Tick();

      const glm::vec2 boundary = static_text_->AddText(
          /*text_index=*/0, kTextHeight, kTextBaseX, kTextBaseY,
          Text::Align::kLeft);
      dynamic_text_->AddText(std::to_string(timer_.frame_rate()), kTextHeight,
                             boundary.y, kTextBaseY, Text::Align::kLeft);

      const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();
      const glm::mat4 model =
          glm::rotate(glm::mat4{1.0f}, elapsed_time * glm::radians(90.0f),
                      glm::vec3{1.0f, 1.0f, 0.0f});
      const glm::mat4 view = glm::lookAt(glm::vec3{3.0f}, glm::vec3{0.0f},
                                         glm::vec3{0.0f, 0.0f, 1.0f});
      const glm::mat4 proj = glm::perspective(
          glm::radians(45.0f), window_.original_aspect_ratio(), 0.1f, 100.0f);
      const Transformation trans{proj * view * model};

      graph.Reset();
      const ImageHandle backbuffer = graph.ImportSwapchainImage(kWindowIndex);
      graph.AddGraphicsPass(
          "CubeAndText",
          [&](PassBuilder& builder) {
            SetupMainPassAttachments(builder, backbuffer, multisampling_mode_,
                                     /*use_depth=*/true);
          },
          [&](PassContext& ctx) {
            const glm::ivec2 frame_size = ctx.GetImageExtent(backbuffer);
            cube_model_->BindPipeline(ctx, frame_size);
            ctx.PushConstants(renderer::ir::shader_stage::VERTEX, /*offset=*/0,
                              sizeof(Transformation), &trans);
            cube_model_->Draw(ctx);

            static_text_->Draw(ctx, frame_size, text_color, kTextAlpha);
            dynamic_text_->Draw(ctx, frame_size, text_color, kTextAlpha);
          });
      renderer_->RenderFrame(kWindowIndex, graph);
    }
    renderer_->WaitIdle();
  }

 private:
  const MultisamplingMode multisampling_mode_;
  common::FrameTimer timer_;
  common::Window window_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<Model> cube_model_;
  std::unique_ptr<StaticText> static_text_;
  std::unique_ptr<DynamicText> dynamic_text_;
};

}  // namespace
}  // namespace lighter::example

int main(int argc, char* argv[]) {
  return lighter::example::ExampleMain<lighter::example::CubeExample>(
      argc, argv, glm::ivec2{800, 600},
      lighter::renderer::ir::MultisamplingMode::kDecent);
}
