//
//  text_util.cc
//
//  Created by Pujun Lun on 6/20/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/extension/text_util.h"

#include <algorithm>

#include "lighter/common/graphics_api.h"
#include "lighter/common/image.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/vulkan/extension/graphics_pass.h"
#include "lighter/renderer/vulkan/wrapper/command.h"
#include "lighter/renderer/vulkan/wrapper/pipeline_util.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

using common::Vertex2D;

enum SubpassIndex {
  kTextSubpassIndex = 0,
  kNumSubpasses,
};

constexpr int kImageBindingPoint = 0;
constexpr uint32_t kVertexBufferBindingPoint = 0;

// Returns the path to font file.
std::string GetFontPath(CharLoader::Font font) {
  using common::file::GetResourcePath;
  switch (font) {
    case CharLoader::Font::kGeorgia:
      return GetResourcePath("font/georgia.ttf");
    case CharLoader::Font::kOstrich:
      return GetResourcePath("font/ostrich.ttf");
  }
}

// Returns the interval between two adjacent characters on the character atlas
// image in number of pixels. We add this interval so that when sampling one
// character, other characters will not affect the result due to numeric errors.
int GetIntervalBetweenChars(const common::CharLib& char_lib) {
  constexpr int kCharWidthToIntervalRatio = 100;
  int total_width = 0;
  for (const auto& pair : char_lib.char_info_map()) {
    if (pair.first != ' ') {
      total_width += pair.second.image.width();
    }
  }
  return std::max(total_width / kCharWidthToIntervalRatio, 1);
}

// Returns descriptor infos for rendering characters.
std::vector<Descriptor::Info> CreateDescriptorInfos() {
  return {
      Descriptor::Info{
          Image::GetDescriptorTypeForSampling(),
          VK_SHADER_STAGE_FRAGMENT_BIT,
          /*bindings=*/{
              Descriptor::Info::Binding{
                  kImageBindingPoint,
                  /*array_length=*/1,
              },
          },
      },
  };
}

// Returns a render pass builder for rendering characters.
std::unique_ptr<RenderPassBuilder> CreateRenderPassBuilder(
    const SharedBasicContext& context) {
  ImageUsageHistory usage_history{ImageUsage{}};
  usage_history
      .AddUsage(kTextSubpassIndex, ImageUsage::GetRenderTargetUsage(0))
      .SetFinalUsage(ImageUsage::GetSampledInFragmentShaderUsage());

  GraphicsPass graphics_pass{context, kNumSubpasses};
  graphics_pass.AddAttachment("Chars", std::move(usage_history),
                              /*get_location=*/[](int subpass) { return 0; });
  return graphics_pass.CreateRenderPassBuilder(/*num_framebuffers=*/1);
}

// Returns a render pass that renders to 'target_image'.
std::unique_ptr<RenderPass> BuildRenderPass(
    const Image& target_image, RenderPassBuilder* render_pass_builder) {
  render_pass_builder->UpdateAttachmentImage(
      /*index=*/0,
      [&target_image](int framebuffer_index) -> const Image& {
          return target_image;
      });
  return render_pass_builder->Build();
}

// Returns a pipeline builder, assuming the per-vertex data is of type Vertex2D.
std::unique_ptr<GraphicsPipelineBuilder> CreatePipelineBuilder(
    const SharedBasicContext& context,
    std::string&& pipeline_name,
    const PerVertexBuffer& vertex_buffer,
    const VkDescriptorSetLayout& descriptor_layout,
    bool enable_color_blend) {
  auto pipeline_builder = std::make_unique<GraphicsPipelineBuilder>(context);

  (*pipeline_builder)
      .SetPipelineName(std::move(pipeline_name))
      .AddVertexInput(kVertexBufferBindingPoint,
                      pipeline::GetPerVertexBindingDescription<Vertex2D>(),
                      vertex_buffer.GetAttributes(/*start_location=*/0))
      .SetPipelineLayout({descriptor_layout}, /*push_constant_ranges=*/{})
      .SetColorBlend({pipeline::GetColorBlendState(enable_color_blend)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetShaderBinaryPath(
                     "text/char.vert", common::api::GraphicsApi::kVulkan))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetShaderBinaryPath(
                     "text/char.frag", common::api::GraphicsApi::kVulkan));

  return pipeline_builder;
}

// Returns a pipeline that renders to 'target_image'.
std::unique_ptr<Pipeline> BuildPipeline(
    const Image& target_image, const VkRenderPass& render_pass,
    GraphicsPipelineBuilder* pipeline_builder) {
  return (*pipeline_builder)
      .SetViewport(pipeline::GetFullFrameViewport(target_image.extent()),
                   /*flip_y=*/false)
      .SetRenderPass(render_pass, kTextSubpassIndex)
      .Build();
}

// Returns texture sampler config for rendering texts.
const ImageSampler::Config& GetTextSamplerConfig() {
  static const auto* config = new ImageSampler::Config{
      VK_FILTER_LINEAR,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
  };
  return *config;
}

// Returns pos in NDC given 2D coordinate in range [0.0, 1.0].
inline glm::vec2 NormalizePos(const glm::vec2& coordinate) {
  return coordinate * 2.0f - 1.0f;
}

} /* namespace */

CharLoader::CharLoader(const SharedBasicContext& context,
                       absl::Span<const std::string> texts,
                       Font font, int font_height) {
  CharImageMap char_image_map;
  {
    const common::CharLib char_lib{
        texts, GetFontPath(font), font_height, /*flip_y=*/true};
    const int interval_between_chars = GetIntervalBetweenChars(char_lib);
    const auto image_usages = {ImageUsage::GetRenderTargetUsage(0),
                               ImageUsage::GetSampledInFragmentShaderUsage()};
    char_atlas_image_ = std::make_unique<OffscreenImage>(
        context, GetCharAtlasImageExtent(char_lib, interval_between_chars),
        common::image::kBwImageChannel, image_usages, GetTextSamplerConfig(),
        /*use_high_precision=*/false);
    space_advance_x_ = GetSpaceAdvanceX(char_lib, *char_atlas_image_);
    CreateCharTextures(context, char_lib, interval_between_chars,
                       *char_atlas_image_, &char_image_map,
                       &char_texture_info_map_);
  }

  std::vector<char> char_merge_order;
  char_merge_order.reserve(char_texture_info_map_.size());
  for (const auto& pair : char_texture_info_map_) {
    char_merge_order.push_back(pair.first);
  }

  const auto vertex_buffer = CreateVertexBuffer(context, char_merge_order);
  const auto descriptor =
      std::make_unique<DynamicDescriptor>(context, CreateDescriptorInfos());

  auto render_pass_builder = CreateRenderPassBuilder(context);
  const auto render_pass = BuildRenderPass(*char_atlas_image_,
                                           render_pass_builder.get());

  auto pipeline_builder =
      CreatePipelineBuilder(context, "Char loader", *vertex_buffer,
                            descriptor->layout(), /*enable_color_blend=*/false);
  const auto pipeline = BuildPipeline(*char_atlas_image_, **render_pass,
                                      pipeline_builder.get());

  const std::vector<RenderPass::RenderOp> render_ops{
      [&](const VkCommandBuffer& command_buffer) {
        pipeline->Bind(command_buffer);
        for (int i = 0; i < char_merge_order.size(); ++i) {
          const auto& char_image =
              char_image_map.find(char_merge_order[i])->second;
          descriptor->PushImageInfos(
              command_buffer, pipeline->layout(), pipeline->binding_point(),
              Image::GetDescriptorTypeForSampling(),
              /*image_info_map=*/{{
                  kImageBindingPoint,
                  {char_image->GetDescriptorInfoForSampling()}},
              });
          vertex_buffer->Draw(command_buffer, kVertexBufferBindingPoint,
                              /*mesh_index=*/i, /*instance_count=*/1);
        }
      },
  };

  const OneTimeCommand command{context, &context->queues().graphics_queue()};
  command.Run(
      [&render_pass, &render_ops](const VkCommandBuffer& command_buffer) {
        render_pass->Run(command_buffer, /*framebuffer_index=*/0, render_ops);
      });
}

VkExtent2D CharLoader::GetCharAtlasImageExtent(
    const common::CharLib& char_lib, int interval_between_chars) const {
  ASSERT_NON_EMPTY(char_lib.char_info_map(), "No character loaded");
  int total_width = 0, height = 0;
  for (const auto& pair : char_lib.char_info_map()) {
    if (pair.first != ' ') {
      total_width += pair.second.image.width() + interval_between_chars;
      height = std::max(height, pair.second.image.height());
    }
  }
  total_width -= interval_between_chars;
  return VkExtent2D{
      static_cast<uint32_t>(total_width),
      static_cast<uint32_t>(height),
  };
}

std::optional<float> CharLoader::GetSpaceAdvanceX(
    const common::CharLib& char_lib, const Image& target_image) const {
  const auto iter = char_lib.char_info_map().find(' ');
  std::optional<float> space_advance;
  if (iter != char_lib.char_info_map().end()) {
    space_advance = static_cast<float>(iter->second.advance.x) /
                    target_image.extent().width;
  }
  return space_advance;
}

void CharLoader::CreateCharTextures(
    const SharedBasicContext& context,
    const common::CharLib& char_lib,
    int interval_between_chars, const Image& target_image,
    CharImageMap* char_image_map,
    CharTextureInfoMap* char_texture_info_map) const {
  const glm::vec2 ratio = 1.0f / util::ExtentToVec(target_image.extent());
  const float normalized_interval =
      static_cast<float>(interval_between_chars) * ratio.x;
  const auto image_usages = {ImageUsage::GetSampledInFragmentShaderUsage()};

  float offset_x = 0.0f;
  for (const auto& pair : char_lib.char_info_map()) {
    const char character = pair.first;
    if (character == ' ') {
      continue;
    }

    const auto& char_info = pair.second;
    const auto advance_x = static_cast<float>(char_info.advance.x) * ratio.x;
    const glm::vec2 size =
        glm::vec2{char_info.image.width(), char_info.image.height()} * ratio;
    const glm::vec2 bearing = glm::vec2{char_info.bearing} * ratio;
    char_texture_info_map->insert({
        character, CharTextureInfo{size, bearing, offset_x, advance_x}});
    char_image_map->insert({
        character,
        std::make_unique<TextureImage>(context, /*generate_mipmaps=*/false,
                                       char_info.image, image_usages,
                                       GetTextSamplerConfig()),
    });
    offset_x += size.x + normalized_interval;
  }
}

std::unique_ptr<StaticPerVertexBuffer> CharLoader::CreateVertexBuffer(
    const SharedBasicContext& context,
    const std::vector<char>& char_merge_order) const {
  std::vector<Vertex2D> vertices;
  vertices.reserve(text::kNumVerticesPerRect * char_merge_order.size());
  for (const auto character : char_merge_order) {
    const auto& texture_info = char_texture_info_map_.find(character)->second;
    text::AppendCharPosAndTexCoord(
        /*pos_bottom_left=*/
        {texture_info.offset_x, 0.0f},
        /*pos_increment=*/texture_info.size,
        /*tex_coord_bottom_left=*/glm::vec2{0.0f},
        /*tex_coord_increment=*/glm::vec2{1.0f},
        &vertices);
  }

  return std::make_unique<StaticPerVertexBuffer>(
      context, PerVertexBuffer::ShareIndicesDataInfo{
          /*num_meshes=*/static_cast<int>(char_merge_order.size()),
          /*per_mesh_vertices=*/
          {vertices, /*num_units_per_mesh=*/text::kNumVerticesPerRect},
          /*shared_indices=*/
          {PerVertexBuffer::VertexDataInfo{text::GetIndicesPerRect()}},
      },
      pipeline::GetVertexAttributes<Vertex2D>()
  );
}

TextLoader::TextLoader(const SharedBasicContext& context,
                       absl::Span<const std::string> texts,
                       CharLoader::Font font, int font_height) {
  const auto& longest_text = std::max_element(
      texts.begin(), texts.end(),
      [](const std::string& lhs, const std::string& rhs) {
        return lhs.length() > rhs.length();
      });
  DynamicPerVertexBuffer vertex_buffer{
      context, text::GetVertexDataSize(longest_text->length()),
      pipeline::GetVertexAttributes<Vertex2D>()};

  auto descriptor = std::make_unique<StaticDescriptor>(
      context, CreateDescriptorInfos());
  auto render_pass_builder = CreateRenderPassBuilder(context);
  // Advance can be negative, and thus bounding boxes of characters may have
  // overlap, hence we need to enable color blending.
  auto pipeline_builder =
      CreatePipelineBuilder(context, "Text loader", vertex_buffer,
                            descriptor->layout(), /*enable_color_blend=*/true);

  const CharLoader char_loader{context, texts, font, font_height};
  text_texture_infos_.reserve(texts.size());
  for (const auto& text : texts) {
    text_texture_infos_.push_back(
        CreateTextTexture(context, text, font_height, char_loader,
                          descriptor.get(), render_pass_builder.get(),
                          pipeline_builder.get(), &vertex_buffer));
  }
}

TextLoader::TextTextureInfo TextLoader::CreateTextTexture(
    const SharedBasicContext& context,
    const std::string& text, int font_height,
    const CharLoader& char_loader,
    StaticDescriptor* descriptor,
    RenderPassBuilder* render_pass_builder,
    GraphicsPipelineBuilder* pipeline_builder,
    DynamicPerVertexBuffer* vertex_buffer) const {
  float total_advance_x = 0.0f;
  float highest_base_y = 0.0f;
  for (const auto character : text) {
    if (character == ' ') {
      total_advance_x += char_loader.space_advance();
    } else {
      const auto& texture_info = char_loader.char_texture_info(character);
      total_advance_x += texture_info.advance_x;
      highest_base_y = std::max(highest_base_y,
                                texture_info.size.y - texture_info.bearing.y);
    }
  }

  // In the coordinate of character atlas image, the width of 'text' is
  // 'total_advance_x' and the height is 1.0. Note that the character atlas
  // image itself is also rescaled in the horizontal direction, hence we
  // should also consider its aspect ratio. The height of text texture will be
  // made 'font_height'.
  const glm::vec2 ratio = 1.0f / glm::vec2{total_advance_x, 1.0f};
  const VkExtent2D text_image_extent{
      static_cast<uint32_t>((total_advance_x * char_loader.GetAspectRatio()) *
                            (static_cast<float>(font_height) / 1.0f)),
      static_cast<uint32_t>(font_height),
  };
  const float base_y = highest_base_y;
  const auto image_usages = {ImageUsage::GetRenderTargetUsage(0),
                             ImageUsage::GetSampledInFragmentShaderUsage()};
  auto text_image = std::make_unique<OffscreenImage>(
      context, text_image_extent, common::image::kBwImageChannel, image_usages,
      GetTextSamplerConfig(), /*use_high_precision=*/false);

  std::vector<Vertex2D> vertices;
  text::LoadCharsVertexData(text, char_loader, ratio, /*initial_offset_x=*/0.0f,
                            base_y, &vertices);
  vertex_buffer->CopyHostData(PerVertexBuffer::ShareIndicesDataInfo{
      /*num_meshes=*/static_cast<int>(text.length()),
      /*per_mesh_vertices=*/
      {vertices, /*num_units_per_mesh=*/text::kNumVerticesPerRect},
      /*shared_indices=*/
      {PerVertexBuffer::VertexDataInfo{text::GetIndicesPerRect()}},
  });

  descriptor->UpdateImageInfos(
      Image::GetDescriptorTypeForSampling(),
      /*image_info_map=*/{
          {kImageBindingPoint,
           {char_loader.atlas_image()->GetDescriptorInfoForSampling()}},
  });
  const auto render_pass = BuildRenderPass(*text_image, render_pass_builder);
  const auto pipeline = BuildPipeline(*text_image, **render_pass,
                                      pipeline_builder);

  const std::vector<RenderPass::RenderOp> render_ops{
      [&](const VkCommandBuffer& command_buffer) {
        pipeline->Bind(command_buffer);
        descriptor->Bind(command_buffer, pipeline->layout(),
                         pipeline->binding_point());
        for (int i = 0; i < text.length(); ++i) {
          vertex_buffer->Draw(command_buffer, kVertexBufferBindingPoint,
                              /*mesh_index=*/i, /*instance_count=*/1);
        }
      },
  };

  const OneTimeCommand command{context, &context->queues().graphics_queue()};
  command.Run(
      [&render_pass, &render_ops](const VkCommandBuffer& command_buffer) {
        render_pass->Run(command_buffer, /*framebuffer_index=*/0, render_ops);
      });

  return TextTextureInfo{util::GetAspectRatio(text_image_extent), base_y,
                         std::move(text_image)};
}

namespace text {

const std::array<uint32_t, kNumIndicesPerRect>& GetIndicesPerRect() {
  static const auto* indices_per_rect =
      new std::array<uint32_t, kNumIndicesPerRect>{0, 1, 2, 0, 2, 3};
  return *indices_per_rect;
}

void AppendCharPosAndTexCoord(const glm::vec2& pos_bottom_left,
                              const glm::vec2& pos_increment,
                              const glm::vec2& tex_coord_bottom_left,
                              const glm::vec2& tex_coord_increment,
                              std::vector<Vertex2D>* vertices) {
  const glm::vec2 pos_top_right = pos_bottom_left + pos_increment;
  const glm::vec2 tex_coord_top_right = tex_coord_bottom_left +
                                        tex_coord_increment;
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
  vertices->reserve(
      vertices->size() + text::kNumVerticesPerRect * text.length());
  for (const auto character : text) {
    if (character == ' ') {
      offset_x += char_loader.space_advance() * ratio.x;
      continue;
    }
    const auto& texture_info = char_loader.char_texture_info(character);
    const glm::vec2& size_in_tex = texture_info.size;
    text::AppendCharPosAndTexCoord(
        /*pos_bottom_left=*/
        {offset_x + texture_info.bearing.x * ratio.x,
         base_y + (texture_info.bearing.y - size_in_tex.y) * ratio.y},
        /*pos_increment=*/size_in_tex * ratio,
        /*tex_coord_bottom_left=*/
        {texture_info.offset_x, 0.0f},
        /*tex_coord_increment=*/size_in_tex,
        vertices);
    offset_x += texture_info.advance_x * ratio.x;
  }
  return offset_x;
}

} /* namespace text */
} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
