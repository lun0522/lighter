//
//  button.cc
//
//  Created by Pujun Lun on 11/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/button.h"

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/absl/memory/memory.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

using common::Vertex2D;
using std::vector;

enum SubpassIndex {
  kBackgroundSubpassIndex = 0,
  kTextSubpassIndex,
  kNumSubpasses,
};

enum VertexBufferBindingPoint {
  kPerVertexBufferBindingPoint = 0,
  kPerInstanceBufferBindingPoint,
};

constexpr int kImageBindingPoint = 0;

vector<Vertex2D> CreatePerVertexData() {
  return {};
}

vector<glm::vec3> CreatePerInstanceData(
    const vector<button::ButtonInfo>& button_infos) {
  using button::ButtonInfo;
  vector<glm::vec3> data;
  data.reserve(button_infos.size() * ButtonInfo::kNumStates);
  for (const auto& info : button_infos) {
    data.emplace_back(info.colors[ButtonInfo::kUnselected]);
    data.emplace_back(info.colors[ButtonInfo::kSelected]);
  }
  return data;
}

} /* namespace */

Button::Button(const SharedBasicContext& context,
               Text::Font font, int font_height,
               const glm::vec3& text_color, float text_alpha,
               const glm::vec2& button_size, float button_alpha,
               const vector<button::ButtonInfo>& button_infos) {
  const int num_buttons = button_infos.size();
  vector<std::string> texts;
  texts.reserve(num_buttons);
  for (const auto& info : button_infos) {
    texts.emplace_back(info.text);
  }
  DynamicText text_renderer{context, /*num_frames_in_flight=*/1,
                            /*viewport_aspect_ratio=*/1.0f,
                            texts, font, font_height};

  const SharedTexture button_image{
      context, common::file::GetResourcePath("texture/rect_rounded.jpg")};
  buttons_image_ = absl::make_unique<OffscreenImage>(
      context, /*channel=*/4, VkExtent2D{
          button_image->extent().width * button::ButtonInfo::kNumStates,
          static_cast<uint32_t>(button_image->extent().height * num_buttons),
      });

  const StaticDescriptor descriptor{
      context, /*infos=*/{
          Descriptor::Info{
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              VK_SHADER_STAGE_FRAGMENT_BIT,
              /*bindings=*/{
                  Descriptor::Info::Binding{
                      kImageBindingPoint,
                      /*array_length=*/1,
                  }},
          }}
  };

  const auto per_vertex_data = CreatePerVertexData();
  const PerVertexBuffer::NoIndicesDataInfo vertex_data_info{
      /*per_mesh_vertices=*/{{PerVertexBuffer::VertexDataInfo{per_vertex_data}}}
  };
  const StaticPerVertexBuffer per_vertex_buffer{
    context, vertex_data_info, pipeline::GetVertexAttribute<Vertex2D>()};

  const auto per_instance_data = CreatePerInstanceData(button_infos);
  vector<VertexBuffer::Attribute> per_instance_attribs{
      {/*offset=*/0, VK_FORMAT_R32G32B32_SFLOAT}};
  const PerInstanceBuffer per_instance_buffer{
    context, per_instance_data, std::move(per_instance_attribs)};

  auto pipeline = PipelineBuilder{context}
      .SetName("make button")
      .AddVertexInput(kPerVertexBufferBindingPoint,
                      pipeline::GetPerVertexBindingDescription<Vertex2D>(),
                      per_vertex_buffer.GetAttributes(/*start_location=*/0))
      .AddVertexInput(kPerInstanceBufferBindingPoint,
                      pipeline::GetPerInstanceBindingDescription<glm::vec3>(),
                      per_instance_buffer.GetAttributes(/*start_location=*/2))
      .SetPipelineLayout({descriptor.layout()}, /*push_constant_ranges=*/{})
      .SetColorBlend({pipeline::GetColorBlendState(/*enable_blend=*/false)})
      .SetFrontFaceDirection(/*counter_clockwise=*/false)
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("make_button.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("make_button.frag"))
      .Build();

  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/false,
      /*num_transparent_subpasses=*/0,
      /*num_overlay_subpasses=*/kNumSubpasses,
  };
  NaiveRenderPassBuilder render_pass_builder{
      context, subpass_config, /*num_framebuffers=*/1,
      /*present_to_screen=*/false, /*multisampling_mode=*/absl::nullopt,
  };
  render_pass_builder.mutable_builder()->UpdateAttachmentImage(
      render_pass_builder.color_attachment_index(),
      [this](int framebuffer_index) -> const Image& {
        return *buttons_image_;
      });
  auto render_pass = render_pass_builder->Build();
}

void Button::Draw(const VkCommandBuffer& command_buffer, int frame) const {

}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
