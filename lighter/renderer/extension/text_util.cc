//
//  text_util.cc
//
//  Created by Pujun Lun on 6/20/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/extension/text_util.h"

#include <algorithm>
#include <array>

#include "lighter/common/file.h"
#include "lighter/common/image.h"
#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/buffer_util.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/pipeline_util.h"
#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::renderer::extension {
namespace {

using common::Vertex2D;
using ir::Buffer;
using ir::BufferUsage;
using ir::GraphicsPipelineDescriptor;
using ir::Image;
using ir::ImageUsage;

constexpr uint32_t kImageBindingPoint = 0;
constexpr uint32_t kVertexBufferBindingPoint = 0;

std::string GetFontPath(CharLoader::Font font) {
  switch (font) {
    case CharLoader::Font::kGeorgia:
      return common::file::GetResourcePath("font/georgia.ttf");
    case CharLoader::Font::kOstrich:
      return common::file::GetResourcePath("font/ostrich.ttf");
  }
}

int GetIntervalBetweenChars(const common::CharLib& char_lib) {
  constexpr int kCharWidthToIntervalRatio = 100;
  int total_width = 0;
  for (const auto& [ch, info] : char_lib.char_info_map()) {
    if (ch != ' ') {
      total_width += info.image.width();
    }
  }
  return std::max(total_width / kCharWidthToIntervalRatio, 1);
}

glm::ivec2 ComputeCharAtlasExtent(const common::CharLib& char_lib,
                                  int interval_between_chars) {
  ASSERT_NON_EMPTY(char_lib.char_info_map(), "No character loaded");
  int total_width = 0;
  int max_height = 0;
  for (const auto& [ch, info] : char_lib.char_info_map()) {
    if (ch != ' ') {
      total_width += info.image.width() + interval_between_chars;
      max_height = std::max(max_height, info.image.height());
    }
  }
  total_width -= interval_between_chars;
  return {total_width, max_height};
}

std::unique_ptr<Buffer> CreateQuadBuffer(const ir::Renderer& renderer,
                                         std::span<const Vertex2D> vertices,
                                         std::span<const uint32_t> indices,
                                         size_t* out_index_offset) {
  const size_t vertex_bytes = common::util::GetTotalDataSize(vertices);
  const size_t index_bytes = common::util::GetTotalDataSize(indices);
  *out_index_offset = vertex_bytes;

  auto buffer = renderer.CreateBuffer(
      Buffer::UpdateRate::kLow, vertex_bytes + index_bytes,
      std::array{BufferUsage::GetVertexBufferUsage(
          BufferUsage::UsageType::kIndexAndVertex)});
  buffer->CopyToDevice(std::array{
      Buffer::CopyInfo{vertices.data(), vertex_bytes, /*offset=*/0},
      Buffer::CopyInfo{indices.data(), index_bytes, vertex_bytes},
  });
  return buffer;
}

GraphicsPipelineDescriptor BuildCharBakePipelineDescriptor(
    const glm::ivec2& target_extent, bool enable_blend) {
  return GraphicsPipelineDescriptor{}
      .SetName(enable_blend ? "Text loader" : "Char loader")
      .SetShader(ir::shader_stage::VERTEX,
                 common::file::GetShaderBinaryPath("text/char.vert"))
      .SetShader(ir::shader_stage::FRAGMENT,
                 common::file::GetShaderBinaryPath("text/char.frag"))
      .UseColorAttachment(
          /*location=*/0, enable_blend
                              ? std::optional{ir::pipeline::GetColorBlend()}
                              : std::nullopt)
      .AddVertexInput({
          .input_rate = ir::VertexInputRate::kVertex,
          .binding_point = kVertexBufferBindingPoint,
          .stride = sizeof(Vertex2D),
          .attributes = ir::buffer::CreateAttributesForVertex2D(
              /*loc_pos=*/0, /*loc_tex_coord=*/1),
      })
      .SetViewport(ir::pipeline::GetFullFrameViewport(target_extent),
                   ir::pipeline::GetFullFrameScissor(target_extent),
                   /*flip_y=*/false);
}

inline glm::vec2 NormalizePos(const glm::vec2& coordinate) {
  return coordinate * 2.0f - 1.0f;
}

}  // namespace

CharLoader::CharLoader(ir::Renderer& renderer,
                       std::span<const std::string> texts, Font font,
                       int font_height) {
  const common::CharLib char_lib{texts, GetFontPath(font), font_height,
                                 /*flip_y=*/true};
  const int interval = GetIntervalBetweenChars(char_lib);
  const glm::ivec2 atlas_extent = ComputeCharAtlasExtent(char_lib, interval);

  const std::array atlas_usages{ImageUsage::GetRenderTargetUsage(0),
                                ImageUsage::GetSampledInFragmentShaderUsage()};
  char_atlas_image_ = renderer.CreateColorImage(
      "char_atlas",
      common::Image::Dimension{atlas_extent, common::image::kBwImageChannel},
      ir::MultisamplingMode::kNone, /*high_precision=*/false, atlas_usages);

  const auto space_iter = char_lib.char_info_map().find(' ');
  if (space_iter != char_lib.char_info_map().end()) {
    space_advance_x_ =
        static_cast<float>(space_iter->second.advance.x) / atlas_extent.x;
  }

  const glm::vec2 ratio = 1.0f / glm::vec2{atlas_extent};
  const float normalized_interval = static_cast<float>(interval) * ratio.x;
  const std::array char_tex_usages{
      ImageUsage::GetSampledInFragmentShaderUsage()};

  std::vector<const Image*> ordered_char_images;
  std::vector<std::unique_ptr<Image>> owned_char_images;
  std::vector<Vertex2D> vertices;

  float offset_x = 0.0f;
  for (const auto& [character, char_info] : char_lib.char_info_map()) {
    if (character == ' ') {
      continue;
    }
    const float advance_x = static_cast<float>(char_info.advance.x) * ratio.x;
    const glm::vec2 size =
        glm::vec2{char_info.image.width(), char_info.image.height()} * ratio;
    const glm::vec2 bearing = glm::vec2{char_info.bearing} * ratio;
    char_texture_info_map_.insert(
        {character, CharTextureInfo{size, bearing, offset_x, advance_x}});

    owned_char_images.push_back(renderer.CreateColorImage(
        std::format("glyph_{}", static_cast<int>(character)), char_info.image,
        /*generate_mipmaps=*/false, char_tex_usages));
    ordered_char_images.push_back(owned_char_images.back().get());

    text::AppendCharPosAndTexCoord(
        /*pos_bottom_left=*/{offset_x, 0.0f}, /*pos_increment=*/size,
        /*tex_coord_bottom_left=*/glm::vec2{0.0f},
        /*tex_coord_increment=*/glm::vec2{1.0f}, &vertices);
    offset_x += size.x + normalized_interval;
  }

  const int num_chars = static_cast<int>(ordered_char_images.size());
  if (num_chars == 0) {
    return;
  }

  const std::vector<uint32_t> indices = text::GenerateQuadIndices(num_chars);
  size_t index_offset = 0;
  const auto quad_buffer =
      CreateQuadBuffer(renderer, vertices, indices, &index_offset);
  const auto sampler = text::GetTextSamplerDescriptor();
  const GraphicsPipelineDescriptor pipeline_desc =
      BuildCharBakePipelineDescriptor(atlas_extent, /*enable_blend=*/false);

  ir::RenderGraph graph;
  const ir::ImageHandle atlas_handle =
      graph.ImportImage("char_atlas", *char_atlas_image_, ImageUsage{},
                        ImageUsage::GetSampledInFragmentShaderUsage());
  graph.AddGraphicsPass(
      "CharAtlasBake",
      [&](ir::PassBuilder& builder) {
        builder.ColorAttachment(atlas_handle, /*location=*/0,
                                ir::AttachmentLoadOp::kClear,
                                ir::AttachmentStoreOp::kStore, glm::vec4{0.0f});
      },
      [&](ir::PassContext& ctx) {
        ctx.BindGraphicsPipeline(pipeline_desc);
        ctx.BindVertexBuffer(kVertexBufferBindingPoint, *quad_buffer,
                             /*offset=*/0);
        ctx.BindIndexBuffer(*quad_buffer, index_offset, /*is_uint16=*/false);
        for (int i = 0; i < num_chars; ++i) {
          ctx.BindExternalImage(kImageBindingPoint, *ordered_char_images[i],
                                &sampler);
          ctx.DrawIndexed(text::kNumIndicesPerRect, /*instance_count=*/1,
                          /*first_index=*/i * text::kNumIndicesPerRect);
        }
      });
  renderer.ExecuteRenderGraph(graph);
}

TextLoader::TextLoader(ir::Renderer& renderer,
                       std::span<const std::string> texts,
                       CharLoader::Font font, int font_height) {
  const CharLoader char_loader{renderer, texts, font, font_height};
  text_texture_infos_.reserve(texts.size());
  for (const std::string& text : texts) {
    text_texture_infos_.push_back(
        CreateTextTexture(renderer, text, font_height, char_loader));
  }
}

TextLoader::TextTextureInfo TextLoader::CreateTextTexture(
    ir::Renderer& renderer, const std::string& text, int font_height,
    const CharLoader& char_loader) const {
  float total_advance_x = 0.0f;
  float highest_base_y = 0.0f;
  for (const char character : text) {
    if (character == ' ') {
      total_advance_x += char_loader.space_advance();
    } else {
      const auto& texture_info = char_loader.char_texture_info(character);
      total_advance_x += texture_info.advance_x;
      highest_base_y = std::max(highest_base_y,
                                texture_info.size.y - texture_info.bearing.y);
    }
  }

  const glm::vec2 ratio = 1.0f / glm::vec2{total_advance_x, 1.0f};
  const glm::ivec2 text_extent{
      std::max(1,
               static_cast<int>(total_advance_x * char_loader.GetAspectRatio() *
                                static_cast<float>(font_height))),
      font_height,
  };
  const float base_y = highest_base_y;

  const std::array text_usages{ImageUsage::GetRenderTargetUsage(0),
                               ImageUsage::GetSampledInFragmentShaderUsage()};
  auto text_image = renderer.CreateColorImage(
      std::format("text_{}", text),
      common::Image::Dimension{text_extent, common::image::kBwImageChannel},
      ir::MultisamplingMode::kNone, /*high_precision=*/false, text_usages);

  std::vector<Vertex2D> vertices;
  text::LoadCharsVertexData(text, char_loader, ratio, /*initial_offset_x=*/0.0f,
                            base_y, &vertices);
  const int num_rects =
      static_cast<int>(vertices.size()) / text::kNumVerticesPerRect;

  std::unique_ptr<Buffer> quad_buffer;
  size_t index_offset = 0;
  std::vector<uint32_t> indices;
  if (num_rects > 0) {
    indices = text::GenerateQuadIndices(num_rects);
    quad_buffer = CreateQuadBuffer(renderer, vertices, indices, &index_offset);
  }

  const auto sampler = text::GetTextSamplerDescriptor();
  const GraphicsPipelineDescriptor pipeline_desc =
      BuildCharBakePipelineDescriptor(text_extent, /*enable_blend=*/true);

  ir::RenderGraph graph;
  const ir::ImageHandle target_handle =
      graph.ImportImage("text_target", *text_image, ImageUsage{},
                        ImageUsage::GetSampledInFragmentShaderUsage());
  graph.AddGraphicsPass(
      "TextBake",
      [&](ir::PassBuilder& builder) {
        builder.ColorAttachment(target_handle, /*location=*/0,
                                ir::AttachmentLoadOp::kClear,
                                ir::AttachmentStoreOp::kStore, glm::vec4{0.0f});
      },
      [&](ir::PassContext& ctx) {
        if (num_rects == 0) {
          return;
        }
        ctx.BindGraphicsPipeline(pipeline_desc);
        ctx.BindVertexBuffer(kVertexBufferBindingPoint, *quad_buffer,
                             /*offset=*/0);
        ctx.BindIndexBuffer(*quad_buffer, index_offset, /*is_uint16=*/false);
        ctx.BindExternalImage(kImageBindingPoint, char_loader.atlas_image(),
                              &sampler);
        ctx.DrawIndexed(static_cast<uint32_t>(indices.size()));
      });
  renderer.ExecuteRenderGraph(graph);

  return TextTextureInfo{
      static_cast<float>(text_extent.x) / static_cast<float>(text_extent.y),
      base_y, std::move(text_image)};
}

namespace text {

const std::array<uint32_t, kNumIndicesPerRect>& GetIndicesPerRect() {
  static const auto* indices_per_rect =
      new std::array<uint32_t, kNumIndicesPerRect>{0, 1, 2, 0, 2, 3};
  return *indices_per_rect;
}

std::vector<uint32_t> GenerateQuadIndices(int num_rects) {
  const auto& pattern = GetIndicesPerRect();
  std::vector<uint32_t> indices;
  indices.reserve(static_cast<size_t>(num_rects) * kNumIndicesPerRect);
  for (int i = 0; i < num_rects; ++i) {
    const uint32_t base_vertex = static_cast<uint32_t>(i * kNumVerticesPerRect);
    for (uint32_t idx : pattern) {
      indices.push_back(base_vertex + idx);
    }
  }
  return indices;
}

ir::SamplerDescriptor GetTextSamplerDescriptor() {
  return ir::SamplerDescriptor{
      .filter_type = ir::FilterType::kLinear,
      .address_mode = ir::SamplerAddressMode::kClampToEdge,
  };
}

void AppendCharPosAndTexCoord(const glm::vec2& pos_bottom_left,
                              const glm::vec2& pos_increment,
                              const glm::vec2& tex_coord_bottom_left,
                              const glm::vec2& tex_coord_increment,
                              std::vector<Vertex2D>* vertices) {
  const glm::vec2 pos_top_right = pos_bottom_left + pos_increment;
  const glm::vec2 tex_coord_top_right =
      tex_coord_bottom_left + tex_coord_increment;
  vertices->reserve(vertices->size() + kNumVerticesPerRect);
  vertices->push_back(Vertex2D{
      NormalizePos(pos_bottom_left),
      tex_coord_bottom_left,
  });
  vertices->push_back(Vertex2D{
      NormalizePos({pos_top_right.x, pos_bottom_left.y}),
      {tex_coord_top_right.x, tex_coord_bottom_left.y},
  });
  vertices->push_back(Vertex2D{
      NormalizePos(pos_top_right),
      tex_coord_top_right,
  });
  vertices->push_back(Vertex2D{
      NormalizePos({pos_bottom_left.x, pos_top_right.y}),
      {tex_coord_bottom_left.x, tex_coord_top_right.y},
  });
}

float LoadCharsVertexData(const std::string& text,
                          const CharLoader& char_loader, const glm::vec2& ratio,
                          float initial_offset_x, float base_y,
                          std::vector<Vertex2D>* vertices) {
  float offset_x = initial_offset_x;
  vertices->reserve(vertices->size() + kNumVerticesPerRect * text.length());
  for (const char character : text) {
    if (character == ' ') {
      offset_x += char_loader.space_advance() * ratio.x;
      continue;
    }
    const auto& texture_info = char_loader.char_texture_info(character);
    const glm::vec2& size_in_tex = texture_info.size;
    AppendCharPosAndTexCoord(
        /*pos_bottom_left=*/
        {offset_x + texture_info.bearing.x * ratio.x,
         base_y + (texture_info.bearing.y - size_in_tex.y) * ratio.y},
        /*pos_increment=*/size_in_tex * ratio,
        /*tex_coord_bottom_left=*/{texture_info.offset_x, 0.0f},
        /*tex_coord_increment=*/size_in_tex, vertices);
    offset_x += texture_info.advance_x * ratio.x;
  }
  return offset_x;
}

}  // namespace text
}  // namespace lighter::renderer::extension
