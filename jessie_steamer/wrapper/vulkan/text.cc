//
//  text.cc
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/text.h"

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

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct DrawCharInfo {
  alignas(16) glm::vec4 color_alpha;
};

float GetVerticalOffset(float vertical_base, Text::Align align,
                        float total_width) {
  switch (align) {
    case Text::Align::kLeft:
      return vertical_base;
    case Text::Align::kCenter:
      return vertical_base - total_width / 2.0f;
    case Text::Align::kRight:
      return vertical_base - total_width;
  }
}

} /* namespace */

StaticText::StaticText(SharedBasicContext context,
                       const vector<string>& texts,
                       Font font, int font_height)
    : Text{std::move(context)} {
  CharLoader loader{context, texts, font, font_height};
}

DynamicText::DynamicText(SharedBasicContext context,
                         int num_frame,
                         const vector<string>& texts,
                         Font font, int font_height)
    : Text{std::move(context)},
      char_loader_{context_, texts, font, font_height},
      vertex_buffer_{context_}, uniform_buffer_{context_},
      pipeline_builder_{context_} {
  uniform_buffer_.Init(sizeof(DrawCharInfo), num_frame);

  enum class BindingPoint : int { kUniformBuffer = 0, kTexture };
  vector<Descriptor::Info> descriptor_infos{
      Descriptor::Info{
          /*descriptor_type=*/VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
          /*bindings=*/{
              Descriptor::Info::Binding{
                  /*resource_type=*/common::types::kUniformBuffer,
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
                  /*resource_type=*/common::types::kTextureDiffuse,
                  /*binding_point=*/static_cast<int>(BindingPoint::kTexture),
                  /*array_length=*/1,
              },
          },
      },
  };
  const Descriptor::ImageInfos image_infos{
      {static_cast<int>(BindingPoint::kTexture),
       {char_loader_.texture()->descriptor_info()}},
  };

  descriptors_.reserve(num_frame);
  for (int frame = 0; frame < num_frame; ++frame) {
    descriptors_.emplace_back(
        absl::make_unique<StaticDescriptor>(context_, descriptor_infos));
    descriptors_[frame]->UpdateBufferInfos(
        descriptor_infos[0], {uniform_buffer_.descriptor_info(frame)});
    descriptors_[frame]->UpdateImageInfos(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_infos);
  }

  pipeline_builder_
      .set_vertex_input(
          GetBindingDescriptions({GetPerVertexBindings<VertexAttrib2D>()}),
          GetAttributeDescriptions({GetVertexAttributes<VertexAttrib2D>()}))
      .set_layout({descriptors_[0]->layout()}, /*push_constant_ranges=*/{})
      .enable_alpha_blend();
}

void DynamicText::Update(VkExtent2D frame_size,
                         const RenderPass& render_pass,
                         uint32_t subpass_index) {
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
      .set_render_pass(render_pass, subpass_index)
      .add_shader({VK_SHADER_STAGE_VERTEX_BIT,
                   "jessie_steamer/shader/vulkan/simple_2d.vert.spv"})
      .add_shader({VK_SHADER_STAGE_FRAGMENT_BIT,
                   "jessie_steamer/shader/vulkan/text.frag.spv"})
      .Build();
}

void DynamicText::Draw(const VkCommandBuffer& command_buffer,
                       int frame, VkExtent2D frame_size, const string& text,
                       const glm::vec3& color, float alpha, float height,
                       float horizontal_base, float vertical_base,
                       Align align) {
  uniform_buffer_.data<DrawCharInfo>(frame)->color_alpha =
      glm::vec4(color, alpha);
  uniform_buffer_.Flush(frame);

  const float frame_width_height_ratio =
      static_cast<float>(frame_size.width) / frame_size.height;
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
      total_width_in_tex_coord += found->second.advance;
    }
  }
  const float initial_vertical_offset =
      GetVerticalOffset(vertical_base, align,
                        total_width_in_tex_coord * ratio.x);

  vector<VertexAttrib2D> vertices;
  vertices.reserve(text_util::kNumVerticesPerChar * num_non_space_char);
  float vertical_offset = initial_vertical_offset;
  for (auto c : text) {
    if (c == ' ') {
      vertical_offset += char_loader_.space_advance() * ratio.x;
      continue;
    }
    const auto& texture_info = texture_map.find(c)->second;
    const glm::vec2& size_in_tex = texture_info.size;
    text_util::AppendCharPosAndTexCoord(
        /*pos_bottom_left=*/
        {vertical_offset + texture_info.bearing.x * ratio.x,
         horizontal_base + (texture_info.bearing.y - size_in_tex.y) * ratio.y},
        /*pos_increment=*/size_in_tex * ratio,
        /*tex_coord_bottom_left=*/
        {texture_info.offset_x + texture_info.bearing.x, 0.0f},
        /*tex_coord_increment=*/size_in_tex,
        &vertices);
    vertical_offset += texture_info.advance * ratio.x;
  }
  const auto& indices = text_util::indices_per_char();

  vertex_buffer_.Init(PerVertexBuffer::InfoReuse{
      /*num_mesh=*/num_non_space_char,
      /*per_mesh_vertices=*/
      {vertices, /*unit_count=*/text_util::kNumVerticesPerChar},
      /*shared_indices=*/{indices},
  });

  pipeline_->Bind(command_buffer);
  vkCmdBindDescriptorSets(
      command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipeline_->layout(), /*firstSet=*/0, /*descriptorSetCount=*/1,
      &descriptors_[frame]->set(), /*dynamicOffsetCount=*/0,
      /*pDynamicOffsets=*/nullptr);
  for (int i = 0; i < num_non_space_char; ++i) {
    vertex_buffer_.Draw(command_buffer, /*mesh_index=*/i, /*instance_count=*/1);
  }
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
