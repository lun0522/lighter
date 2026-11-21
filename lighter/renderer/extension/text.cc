//
//  text.cc
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/extension/text.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <utility>

#include "lighter/common/file.h"
#include "lighter/common/util.h"
#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/ir/pipeline_util.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::renderer::extension {
namespace {

using common::Vertex2D;
using ir::Buffer;
using ir::BufferUsage;

constexpr uint32_t kUniformBindingPoint = 0;
constexpr uint32_t kTextureBindingPoint = 1;
constexpr uint32_t kVertexBufferBindingPoint = 0;
constexpr int kInitialRectCapacity = 16;

// Consistent with text/text.frag uniform block layout (std140).
struct TextRenderInfo {
  alignas(sizeof(float) * 4) glm::vec4 color_alpha;
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

glm::vec2 AbsX(const glm::vec2& value) { return {std::abs(value.x), value.y}; }

size_t GetQuadBufferBytes(int rect_capacity) {
  return sizeof(Vertex2D) * text::kNumVerticesPerRect * rect_capacity +
         sizeof(uint32_t) * text::kNumIndicesPerRect * rect_capacity;
}

ir::GraphicsPipelineDescriptor BuildBasePipelineDescriptor(
    std::string pipeline_name) {
  ir::GraphicsPipelineDescriptor desc;
  desc.SetName(std::move(pipeline_name))
      .SetShader(ir::shader_stage::VERTEX,
                 common::file::GetShaderBinaryPath("text/char.vert"))
      .SetShader(ir::shader_stage::FRAGMENT,
                 common::file::GetShaderBinaryPath("text/text.frag"))
      .AddVertexInput({
          .input_rate = ir::VertexInputRate::kVertex,
          .binding_point = kVertexBufferBindingPoint,
          .stride = sizeof(Vertex2D),
          .attributes =
              {
                  {0, ir::DataFormat::kSFloat32Vec2, offsetof(Vertex2D, pos)},
                  {1, ir::DataFormat::kSFloat32Vec2,
                   offsetof(Vertex2D, tex_coord)},
              },
      });
  return desc;
}

}  // namespace

Text::Text(const ir::Renderer& renderer, std::string pipeline_name,
           float viewport_aspect_ratio, int num_frames_in_flight)
    : renderer_{renderer},
      viewport_aspect_ratio_{viewport_aspect_ratio},
      base_pipeline_descriptor_{
          BuildBasePipelineDescriptor(std::move(pipeline_name))} {
  ASSERT_TRUE(num_frames_in_flight > 0,
              "num_frames_in_flight must be positive");

  const std::array uniform_usages{
      BufferUsage::GetUniformBufferUsage(ir::AccessLocation::kFragmentShader)};
  const std::array quad_usages{BufferUsage::GetVertexBufferUsage(
      BufferUsage::UsageType::kIndexAndVertex)};
  frame_buffers_.resize(num_frames_in_flight);
  for (FrameBuffers& fb : frame_buffers_) {
    fb.uniform_buffer = renderer_.CreateBuffer(
        Buffer::UpdateRate::kHigh, sizeof(TextRenderInfo), uniform_usages);
    fb.rect_capacity = kInitialRectCapacity;
    fb.quad_buffer = renderer_.CreateBuffer(
        Buffer::UpdateRate::kHigh, GetQuadBufferBytes(fb.rect_capacity),
        quad_usages);
  }
}

ir::GraphicsPipelineDescriptor Text::BuildPipelineDescriptor(
    const glm::ivec2& frame_size, bool flip_y,
    int num_color_attachments) const {
  ir::GraphicsPipelineDescriptor desc = base_pipeline_descriptor_;
  const ir::GraphicsPipelineDescriptor::ColorBlend alpha_blend =
      ir::pipeline::GetColorAlphaBlend();
  for (int i = 0; i < num_color_attachments; ++i) {
    desc.UseColorAttachment(i, alpha_blend);
  }
  desc.SetViewport(
      ir::pipeline::GetViewport(frame_size, viewport_aspect_ratio_),
      ir::pipeline::GetFullFrameScissor(frame_size), flip_y);
  return desc;
}

Text::PreparedFrameBuffers Text::PrepareFrameBuffers(const glm::vec3& color,
                                                     float alpha) {
  const int num_rects =
      static_cast<int>(vertices_to_draw_.size()) / text::kNumVerticesPerRect;
  if (num_rects == 0) {
    vertices_to_draw_.clear();
    return {};
  }

  FrameBuffers& fb = frame_buffers_[current_frame_slot_];
  current_frame_slot_ =
      (current_frame_slot_ + 1) % static_cast<int>(frame_buffers_.size());

  const TextRenderInfo render_info{glm::vec4{color, alpha}};
  fb.uniform_buffer->CopyToDevice(std::array{
      Buffer::CopyInfo{&render_info, sizeof(TextRenderInfo), /*offset=*/0},
  });

  if (num_rects > fb.rect_capacity) {
    fb.rect_capacity = std::max(num_rects, fb.rect_capacity * 2);
    fb.quad_buffer = renderer_.CreateBuffer(
        Buffer::UpdateRate::kHigh, GetQuadBufferBytes(fb.rect_capacity),
        std::array{BufferUsage::GetVertexBufferUsage(
            BufferUsage::UsageType::kIndexAndVertex)});
  }

  const size_t vertex_bytes = common::util::GetTotalDataSize(
      std::span<const Vertex2D>{vertices_to_draw_});
  const std::vector<uint32_t> indices = text::GenerateQuadIndices(num_rects);
  const size_t index_bytes =
      common::util::GetTotalDataSize(std::span<const uint32_t>{indices});
  fb.quad_buffer->CopyToDevice(std::array{
      Buffer::CopyInfo{vertices_to_draw_.data(), vertex_bytes, /*offset=*/0},
      Buffer::CopyInfo{indices.data(), index_bytes, vertex_bytes},
  });
  vertices_to_draw_.clear();

  return PreparedFrameBuffers{
      .num_rects = num_rects,
      .uniform_buffer = fb.uniform_buffer.get(),
      .quad_buffer = fb.quad_buffer.get(),
      .index_offset = vertex_bytes,
  };
}

StaticText::StaticText(ir::Renderer& renderer, float viewport_aspect_ratio,
                       std::span<const std::string> texts, Font font,
                       int font_height, int num_frames_in_flight)
    : Text{renderer, "Static text", viewport_aspect_ratio,
           num_frames_in_flight},
      text_loader_{renderer, texts, font, font_height} {}

glm::vec2 StaticText::AddText(int text_index, float height, float base_x,
                              float base_y, Align align) {
  texts_to_draw_.push_back(text_index);
  const auto& texture_info = text_loader_.texture_info(text_index);
  const glm::vec2 ratio = AbsX(
      glm::vec2{texture_info.aspect_ratio / viewport_aspect_ratio(), 1.0f} *
      height);
  const float width_in_frame = ratio.x;
  const float offset_x = GetOffsetX(base_x, align, width_in_frame);
  text::AppendCharPosAndTexCoord(
      /*pos_bottom_left=*/{offset_x, base_y - texture_info.base_y * ratio.y},
      /*pos_increment=*/ratio,
      /*tex_coord_bottom_left=*/glm::vec2{0.0f},
      /*tex_coord_increment=*/glm::vec2{1.0f}, mutable_vertices());
  return {offset_x, offset_x + width_in_frame};
}

void StaticText::Draw(ir::PassContext& ctx, const glm::ivec2& frame_size,
                      const glm::vec3& color, float alpha, bool flip_y) {
  const PreparedFrameBuffers prepared = PrepareFrameBuffers(color, alpha);
  if (prepared.num_rects == 0) {
    texts_to_draw_.clear();
    return;
  }
  ASSERT_TRUE(prepared.num_rects == static_cast<int>(texts_to_draw_.size()),
              std::format("Expected number of texts: {} vs {}",
                          prepared.num_rects, texts_to_draw_.size()));

  ctx.BindGraphicsPipeline(BuildPipelineDescriptor(frame_size, flip_y));
  ctx.BindVertexBuffer(kVertexBufferBindingPoint, *prepared.quad_buffer,
                       /*offset=*/0);
  ctx.BindIndexBuffer(*prepared.quad_buffer, prepared.index_offset,
                      /*is_uint16=*/false);
  ctx.BindBuffer(kUniformBindingPoint, *prepared.uniform_buffer);

  const ir::SamplerDescriptor sampler = text::GetTextSamplerDescriptor();
  for (int i = 0; i < prepared.num_rects; ++i) {
    ctx.BindExternalImage(kTextureBindingPoint,
                          *text_loader_.texture_info(texts_to_draw_[i]).image,
                          &sampler);
    ctx.DrawIndexed(text::kNumIndicesPerRect, /*instance_count=*/1,
                    /*first_index=*/i * text::kNumIndicesPerRect);
  }
  texts_to_draw_.clear();
}

DynamicText::DynamicText(ir::Renderer& renderer, float viewport_aspect_ratio,
                         std::span<const std::string> texts, Font font,
                         int font_height, int num_frames_in_flight)
    : Text{renderer, "Dynamic text", viewport_aspect_ratio,
           num_frames_in_flight},
      char_loader_{renderer, texts, font, font_height} {}

glm::vec2 DynamicText::AddText(const std::string& text, float height,
                               float base_x, float base_y, Align align) {
  const glm::vec2 ratio = AbsX(
      glm::vec2{char_loader_.GetAspectRatio() / viewport_aspect_ratio(), 1.0f} *
      height);
  float total_width_in_tex_coord = 0.0f;
  for (const char character : text) {
    if (character == ' ') {
      total_width_in_tex_coord += char_loader_.space_advance();
    } else {
      total_width_in_tex_coord +=
          char_loader_.char_texture_info(character).advance_x;
    }
  }

  const float initial_offset_x =
      GetOffsetX(base_x, align, total_width_in_tex_coord * ratio.x);
  const float final_offset_x = text::LoadCharsVertexData(
      text, char_loader_, ratio, initial_offset_x, base_y, mutable_vertices());
  return {initial_offset_x, final_offset_x};
}

void DynamicText::Draw(ir::PassContext& ctx, const glm::ivec2& frame_size,
                       const glm::vec3& color, float alpha, bool flip_y) {
  const PreparedFrameBuffers prepared = PrepareFrameBuffers(color, alpha);
  if (prepared.num_rects == 0) {
    return;
  }

  ctx.BindGraphicsPipeline(BuildPipelineDescriptor(frame_size, flip_y));
  ctx.BindVertexBuffer(kVertexBufferBindingPoint, *prepared.quad_buffer,
                       /*offset=*/0);
  ctx.BindIndexBuffer(*prepared.quad_buffer, prepared.index_offset,
                      /*is_uint16=*/false);
  ctx.BindBuffer(kUniformBindingPoint, *prepared.uniform_buffer);

  const ir::SamplerDescriptor sampler = text::GetTextSamplerDescriptor();
  ctx.BindExternalImage(kTextureBindingPoint, char_loader_.atlas_image(),
                        &sampler);
  ctx.DrawIndexed(
      static_cast<uint32_t>(prepared.num_rects * text::kNumIndicesPerRect));
}

}  // namespace lighter::renderer::extension
