//
//  text.cc
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/text.h"

#include <algorithm>

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/util.h"
#include "jessie_steamer/wrapper/vulkan/vertex_input_util.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::string;
using std::vector;

using common::VertexAttrib2D;

enum class BindingPoint : int { kUniformBuffer = 0, kTexture };

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct TextRenderInfo {
  alignas(16) glm::vec4 color_alpha;
};

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

const vector<Descriptor::Info>& CreateDescriptorInfos() {
  vector<Descriptor::Info>* descriptor_infos = nullptr;
  if (descriptor_infos == nullptr) {
    descriptor_infos = new vector<Descriptor::Info>{
        Descriptor::Info{
            /*descriptor_type=*/VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
            /*bindings=*/{
                Descriptor::Info::Binding{
                    /*binding_point=*/
                    static_cast<int>(BindingPoint::kUniformBuffer),
                    /*array_length=*/1,
                },
            },
        },
        Descriptor::Info{
            /*descriptor_type=*/VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
            /*bindings=*/{
                Descriptor::Info::Binding{
                    /*binding_point=*/static_cast<int>(BindingPoint::kTexture),
                    /*array_length=*/1,
                },
            },
        },
    };
  }
  return *descriptor_infos;
}

} /* namespace */

Text::Text(SharedBasicContext context, int num_frame)
    : context_{std::move(context)},
      vertex_buffer_{context_, text_util::GetVertexDataSize(/*num_rect=*/1)},
      pipeline_builder_{context_} {
  uniform_buffer_ = absl::make_unique<UniformBuffer>(
      context_, sizeof(TextRenderInfo), num_frame);
  pipeline_builder_
      .set_vertex_input(
          GetBindingDescriptions({GetPerVertexBindings<VertexAttrib2D>()}),
          GetAttributeDescriptions({GetVertexAttributes<VertexAttrib2D>()}))
      .enable_alpha_blend();
}

void Text::Update(VkExtent2D frame_size,
                  const RenderPass& render_pass, uint32_t subpass_index) {
  using common::file::GetShaderPath;
  pipeline_ = pipeline_builder_
      .set_viewport({
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
          },
      })
      .set_render_pass(*render_pass, subpass_index)
      .add_shader({VK_SHADER_STAGE_VERTEX_BIT,
                   GetShaderPath("vulkan/simple_2d.vert.spv")})
      .add_shader({VK_SHADER_STAGE_FRAGMENT_BIT,
                   GetShaderPath("vulkan/text.frag.spv")})
      .Build();
}

void Text::UpdateUniformBuffer(int frame, const glm::vec3& color, float alpha) {
  uniform_buffer_->HostData<TextRenderInfo>(frame)->color_alpha =
      glm::vec4(color, alpha);
  uniform_buffer_->Flush(frame);
}

StaticText::StaticText(SharedBasicContext context,
                       int num_frame,
                       const vector<string>& texts,
                       Font font, int font_height)
    : Text{std::move(context), num_frame},
      text_loader_{context_, texts, font, font_height} {
  descriptors_.reserve(num_frame);
  push_descriptors_.reserve(num_frame);

  const auto& descriptor_infos = CreateDescriptorInfos();
  for (int frame = 0; frame < num_frame; ++frame) {
    descriptors_.emplace_back(
        absl::make_unique<DynamicDescriptor>(context_, descriptor_infos));
    push_descriptors_.emplace_back(
        [=](const VkCommandBuffer& command_buffer,
            const VkPipelineLayout& pipeline_layout,
            int text_index) {
          descriptors_[frame]->PushBufferInfos(
              command_buffer, pipeline_layout,
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              {{static_cast<uint32_t>(BindingPoint::kUniformBuffer),
                {uniform_buffer_->GetDescriptorInfo(frame)}}});
          descriptors_[frame]->PushImageInfos(
              command_buffer, pipeline_layout,
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              {{static_cast<int>(BindingPoint::kTexture),
                {text_loader_.texture(text_index).image
                     ->GetDescriptorInfo()}}});
        });
  }

  pipeline_builder_.set_layout({descriptors_[0]->layout()},
                               /*push_constant_ranges=*/{});
}

glm::vec2 StaticText::Draw(const VkCommandBuffer& command_buffer,
                           int frame, VkExtent2D frame_size, int text_index,
                           const glm::vec3& color, float alpha, float height,
                           float base_x, float base_y, Align align) {
  UpdateUniformBuffer(frame, color, alpha);

  const auto& texture = text_loader_.texture(text_index);
  const float frame_width_height_ratio = util::GetWidthHeightRatio(frame_size);
  const glm::vec2 ratio = glm::vec2{texture.width_height_ratio /
                                    frame_width_height_ratio, 1.0f} *
                          (height / 1.0f);
  const float width_in_frame = 1.0f * ratio.x;
  const float offset_x = GetOffsetX(base_x, align, width_in_frame);

  vector<VertexAttrib2D> vertices;
  vertices.reserve(text_util::kNumVerticesPerRect);
  text_util::AppendCharPosAndTexCoord(
      /*pos_bottom_left=*/{offset_x, base_y - texture.base_y * ratio.y},
      /*pos_increment=*/glm::vec2{1.0f} * ratio,
      /*tex_coord_bottom_left=*/glm::vec2{0.0f},
      /*tex_coord_increment=*/glm::vec2{1.0f},
      &vertices);
  const PerVertexBuffer::NoShareIndicesDataInfo::PerMeshInfo mesh_info{
      PerVertexBuffer::DataInfo{text_util::GetIndicesPerRect()},
      PerVertexBuffer::DataInfo{vertices},
  };
  vertex_buffer_.Allocate(PerVertexBuffer::NoShareIndicesDataInfo{{mesh_info}});

  pipeline_->Bind(command_buffer);
  push_descriptors_[frame](command_buffer, pipeline_->layout(), text_index);
  vertex_buffer_.Draw(command_buffer, /*mesh_index=*/0, /*instance_count=*/1);

  return glm::vec2{offset_x, offset_x + width_in_frame};
}

DynamicText::DynamicText(SharedBasicContext context,
                         int num_frame,
                         const vector<string>& texts,
                         Font font, int font_height)
    : Text{std::move(context), num_frame},
      char_loader_{context_, texts, font, font_height} {
  const auto& descriptor_infos = CreateDescriptorInfos();
  const Descriptor::ImageInfoMap image_info_map{
      {static_cast<int>(BindingPoint::kTexture),
       {char_loader_.texture()->GetDescriptorInfo()}},
  };

  descriptors_.reserve(num_frame);
  for (int frame = 0; frame < num_frame; ++frame) {
    descriptors_.emplace_back(
        absl::make_unique<StaticDescriptor>(context_, descriptor_infos));
    descriptors_[frame]->UpdateBufferInfos(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, {
            {static_cast<int>(BindingPoint::kUniformBuffer),
             {uniform_buffer_->GetDescriptorInfo(frame)}}
        });
    descriptors_[frame]->UpdateImageInfos(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info_map);
  }

  pipeline_builder_.set_layout({descriptors_[0]->layout()},
                                /*push_constant_ranges=*/{});
}

glm::vec2 DynamicText::Draw(
    const VkCommandBuffer& command_buffer, int frame, VkExtent2D frame_size,
    const string& text, const glm::vec3& color, float alpha, float height,
    float base_x, float base_y, Align align) {
  UpdateUniformBuffer(frame, color, alpha);

  const float frame_width_height_ratio = util::GetWidthHeightRatio(frame_size);
  const glm::vec2 ratio = glm::vec2{char_loader_.width_height_ratio() /
                                    frame_width_height_ratio, 1.0f} *
                          (height / 1.0f);
  const auto& texture_map = char_loader_.char_texture_map();

  float total_width_in_tex_coord = 0.0f;
  int num_non_space_char = 0;
  for (auto c : text) {
    if (c == ' ') {
      if (char_loader_.space_advance() < 0.0f) {
        FATAL("Space was not loaded");
      }
      total_width_in_tex_coord += char_loader_.space_advance();
    } else {
      auto found = texture_map.find(c);
      if (found == texture_map.end()) {
        FATAL(absl::StrFormat("'%c' was not loaded", c));
      }
      ++num_non_space_char;
      total_width_in_tex_coord += found->second.advance_x;
    }
  }

  const float initial_offset_x = GetOffsetX(base_x, align,
                                            total_width_in_tex_coord * ratio.x);
  float final_offset_x = text_util::LoadCharsVertexData(
      text, char_loader_, ratio, initial_offset_x, base_y, /*flip_y=*/false,
      &vertex_buffer_);

  pipeline_->Bind(command_buffer);
  descriptors_[frame]->Bind(command_buffer, pipeline_->layout());
  for (int i = 0; i < num_non_space_char; ++i) {
    vertex_buffer_.Draw(command_buffer, /*mesh_index=*/i, /*instance_count=*/1);
  }

  return glm::vec2{initial_offset_x, final_offset_x};
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
