//
//  text.cc
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/text.h"

#include <algorithm>

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/align.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using common::Vertex2D;
using std::string;
using std::vector;

constexpr uint32_t kVertexBufferBindingPoint = 0;

enum BindingPoint { kUniformBufferBindingPoint = 0, kTextureBindingPoint };

/* BEGIN: Consistent with structs used in shaders. */

struct TextRenderInfo {
  ALIGN_VEC4 glm::vec4 color_alpha;
};

/* END: Consistent with structs used in shaders. */

// Returns the starting horizontal offset.
float GetOffsetX(float base_x, Text::Align align, float total_width) {
  switch (align) {
    case Text::Align::kLeft:
      return base_x;
    case Text::Align::kCenter:
      return base_x - total_width / 2.0f;
    case Text::Align::kRight:
      return base_x - total_width;
  }
}

// Returns descriptor infos for rendering text.
const vector<Descriptor::Info>& GetDescriptorInfos() {
  static vector<Descriptor::Info>* descriptor_infos = nullptr;
  if (descriptor_infos == nullptr) {
    descriptor_infos = new vector<Descriptor::Info>{
        Descriptor::Info{
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            /*bindings=*/{
                Descriptor::Info::Binding{
                    kUniformBufferBindingPoint,
                    /*array_length=*/1,
                },
            },
        },
        Descriptor::Info{
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            /*bindings=*/{
                Descriptor::Info::Binding{
                    kTextureBindingPoint,
                    /*array_length=*/1,
                },
            },
        },
    };
  }
  return *descriptor_infos;
}

} /* namespace */

Text::Text(SharedBasicContext context, int num_frames_in_flight)
    : context_{std::move(context)},
      vertex_buffer_{context_, text_util::GetVertexDataSize(/*num_rects=*/1),
                     pipeline::GetVertexAttribute<Vertex2D>()},
      uniform_buffer_{context_, sizeof(TextRenderInfo), num_frames_in_flight},
      pipeline_builder_{context_} {
  pipeline_builder_.AddVertexInput(
      kVertexBufferBindingPoint,
      pipeline::GetPerVertexBindingDescription<Vertex2D>(),
      vertex_buffer_.GetAttributes(/*start_location=*/0));
}

void Text::Update(const VkExtent2D& frame_size,
                  const RenderPass& render_pass, uint32_t subpass_index) {
  using common::file::GetShaderPath;
  pipeline_ = pipeline_builder_
      .SetViewport(
          /*viewport=*/VkViewport{
              /*x=*/0.0f,
              /*y=*/0.0f,
              static_cast<float>(frame_size.width),
              static_cast<float>(frame_size.height),
              /*minDepth=*/0.0f,
              /*maxDepth=*/1.0f,
          },
          /*scissor=*/VkRect2D{
              /*offset=*/{0, 0},
              frame_size,
          }
      )
      .SetRenderPass(*render_pass, subpass_index)
      .SetColorBlend(
          vector<VkPipelineColorBlendAttachmentState>(
              render_pass.num_color_attachments(subpass_index),
              pipeline::GetColorBlendState(/*enable_blend=*/true)))
      .AddShader(VK_SHADER_STAGE_VERTEX_BIT,
                 GetShaderPath("vulkan/char.vert.spv"))
      .AddShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 GetShaderPath("vulkan/text.frag.spv"))
      .Build();
}

void Text::UpdateUniformBuffer(int frame, const glm::vec3& color, float alpha) {
  uniform_buffer_.HostData<TextRenderInfo>(frame)->color_alpha =
      glm::vec4(color, alpha);
  uniform_buffer_.Flush(frame);
}

StaticText::StaticText(SharedBasicContext context,
                       int num_frames_in_flight,
                       const vector<string>& texts,
                       Font font, int font_height)
    : Text{std::move(context), num_frames_in_flight},
      text_loader_{context_, texts, font, font_height} {
  descriptors_.reserve(num_frames_in_flight);
  push_descriptors_.reserve(num_frames_in_flight);

  const auto& descriptor_infos = GetDescriptorInfos();
  for (int frame = 0; frame < num_frames_in_flight; ++frame) {
    descriptors_.emplace_back(
        absl::make_unique<DynamicDescriptor>(context_, descriptor_infos));
    push_descriptors_.emplace_back(
        [=](const VkCommandBuffer& command_buffer,
            const VkPipelineLayout& pipeline_layout,
            int text_index) {
          descriptors_[frame]->PushBufferInfos(
              command_buffer, pipeline_layout,
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              /*buffer_info_map=*/{{
                  kUniformBufferBindingPoint,
                  {uniform_buffer_.GetDescriptorInfo(frame)},
              }}
          );
          descriptors_[frame]->PushImageInfos(
              command_buffer, pipeline_layout,
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              /*image_info_map=*/{{
                  kTextureBindingPoint,
                  {text_loader_.texture_info(text_index).image
                       ->GetDescriptorInfo()},
              }}
          );
        });
  }

  pipeline_builder_.SetPipelineLayout({descriptors_[0]->layout()},
                                      /*push_constant_ranges=*/{});
}

glm::vec2 StaticText::Draw(const VkCommandBuffer& command_buffer, int frame,
                           const VkExtent2D& frame_size, int text_index,
                           const glm::vec3& color, float alpha, float height,
                           float base_x, float base_y, Align align) {
  UpdateUniformBuffer(frame, color, alpha);

  const auto& texture_info = text_loader_.texture_info(text_index);
  const float frame_width_height_ratio = util::GetWidthHeightRatio(frame_size);
  const glm::vec2 ratio = glm::vec2{texture_info.width_height_ratio /
                                    frame_width_height_ratio, 1.0f} *
                          (height / 1.0f);
  const float width_in_frame = 1.0f * ratio.x;
  const float offset_x = GetOffsetX(base_x, align, width_in_frame);

  vector<Vertex2D> vertices;
  vertices.reserve(text_util::kNumVerticesPerRect);
  text_util::AppendCharPosAndTexCoord(
      /*pos_bottom_left=*/{offset_x, base_y - texture_info.base_y * ratio.y},
      /*pos_increment=*/glm::vec2{1.0f} * ratio,
      /*tex_coord_bottom_left=*/glm::vec2{0.0f},
      /*tex_coord_increment=*/glm::vec2{1.0f},
      &vertices);
  const PerVertexBuffer::NoShareIndicesDataInfo::PerMeshInfo mesh_info{
      PerVertexBuffer::VertexDataInfo{text_util::GetIndicesPerRect()},
      PerVertexBuffer::VertexDataInfo{vertices},
  };
  vertex_buffer_.CopyHostData(
      PerVertexBuffer::NoShareIndicesDataInfo{{mesh_info}});

  pipeline_->Bind(command_buffer);
  push_descriptors_[frame](command_buffer, pipeline_->layout(), text_index);
  vertex_buffer_.Draw(command_buffer, kVertexBufferBindingPoint,
                      /*mesh_index=*/0, /*instance_count=*/1);

  return glm::vec2{offset_x, offset_x + width_in_frame};
}

DynamicText::DynamicText(SharedBasicContext context,
                         int num_frames_in_flight,
                         const vector<string>& texts,
                         Font font, int font_height)
    : Text{std::move(context), num_frames_in_flight},
      char_loader_{context_, texts, font, font_height} {
  const auto& descriptor_infos = GetDescriptorInfos();
  const Descriptor::ImageInfoMap image_info_map{{
      kTextureBindingPoint,
      {char_loader_.library_image()->GetDescriptorInfo()},
  }};

  descriptors_.reserve(num_frames_in_flight);
  for (int frame = 0; frame < num_frames_in_flight; ++frame) {
    descriptors_.emplace_back(
        absl::make_unique<StaticDescriptor>(context_, descriptor_infos));
    descriptors_[frame]->UpdateBufferInfos(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        /*buffer_info_map=*/{{
            kUniformBufferBindingPoint,
            {uniform_buffer_.GetDescriptorInfo(frame)},
        }}
    );
    descriptors_[frame]->UpdateImageInfos(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info_map);
  }

  pipeline_builder_.SetPipelineLayout({descriptors_[0]->layout()},
                                      /*push_constant_ranges=*/{});
}

glm::vec2 DynamicText::Draw(
    const VkCommandBuffer& command_buffer, int frame,
    const VkExtent2D& frame_size, const string& text, const glm::vec3& color,
    float alpha, float height, float base_x, float base_y, Align align) {
  UpdateUniformBuffer(frame, color, alpha);

  const float frame_width_height_ratio = util::GetWidthHeightRatio(frame_size);
  const glm::vec2 ratio = glm::vec2{char_loader_.GetWidthHeightRatio() /
                                    frame_width_height_ratio, 1.0f} *
                          (height / 1.0f);

  float total_width_in_tex_coord = 0.0f;
  int num_non_space_chars = 0;
  for (auto character : text) {
    if (character == ' ') {
      total_width_in_tex_coord += char_loader_.space_advance();
    } else {
      ++num_non_space_chars;
      const auto& texture_info = char_loader_.char_texture_info(character);
      total_width_in_tex_coord += texture_info.advance_x;
    }
  }

  const float initial_offset_x = GetOffsetX(base_x, align,
                                            total_width_in_tex_coord * ratio.x);
  const float final_offset_x =
      text_util::LoadCharsVertexData(
          text, char_loader_, ratio, initial_offset_x, base_y, /*flip_y=*/false,
          &vertex_buffer_);

  pipeline_->Bind(command_buffer);
  descriptors_[frame]->Bind(command_buffer, pipeline_->layout());
  for (int i = 0; i < num_non_space_chars; ++i) {
    vertex_buffer_.Draw(command_buffer, kVertexBufferBindingPoint,
                        /*mesh_index=*/i, /*instance_count=*/1);
  }

  return glm::vec2{initial_offset_x, final_offset_x};
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
