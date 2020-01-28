//
//  text_util.cc
//
//  Created by Pujun Lun on 6/20/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/text_util.h"

#include <algorithm>

#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/absl/memory/memory.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using common::Vertex2D;
using std::array;
using std::string;
using std::vector;

constexpr int kImageBindingPoint = 0;
constexpr int kSingleChannel = 1;
constexpr uint32_t kVertexBufferBindingPoint = 0;

enum SubpassIndex {
  kTextSubpassIndex = 0,
  kNumSubpasses,
  kNumOverlaySubpasses = kNumSubpasses - kTextSubpassIndex,
};

// Returns the path to font file.
string GetFontPath(CharLoader::Font font) {
  using common::file::GetResourcePath;
  switch (font) {
    case CharLoader::Font::kGeorgia:
      return GetResourcePath("font/georgia.ttf");
    case CharLoader::Font::kOstrich:
      return GetResourcePath("font/ostrich.ttf");
  }
}

// Returns the interval between two adjacent characters on the character library
// image in number of pixels. We add this interval so that when sampling one
// character, other characters will not affect the result due to numeric errors.
int GetIntervalBetweenChars(const common::CharLib& char_lib) {
  constexpr int kCharWidthToIntervalRatio = 100;
  int total_width = 0;
  for (const auto& pair : char_lib.char_info_map()) {
    if (pair.first != ' ') {
      total_width += pair.second.image->width;
    }
  }
  return std::max(total_width / kCharWidthToIntervalRatio, 1);
}

// Returns descriptor infos for rendering characters.
vector<Descriptor::Info> CreateDescriptorInfos() {
  return {
      Descriptor::Info{
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
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
std::unique_ptr<NaiveRenderPassBuilder> CreateRenderPassBuilder(
    const SharedBasicContext& context) {
  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/false,
      /*num_transparent_subpasses=*/0,
      kNumOverlaySubpasses,
  };
  return absl::make_unique<NaiveRenderPassBuilder>(
      context, subpass_config, /*num_framebuffers=*/1,
      /*present_to_screen=*/false, /*multisampling_mode=*/absl::nullopt);
}

// Returns a render pass that renders to 'target_image'.
std::unique_ptr<RenderPass> BuildRenderPass(
    const Image& target_image, NaiveRenderPassBuilder* render_pass_builder) {
  render_pass_builder->mutable_builder()->UpdateAttachmentImage(
      render_pass_builder->color_attachment_index(),
      [&target_image](int framebuffer_index) -> const Image& {
          return target_image;
      });
  return (*render_pass_builder)->Build();
}

// Returns a pipeline builder, assuming the per-vertex data is of type Vertex2D,
// and the front face direction is clockwise, since we will flip Y coordinates.
std::unique_ptr<PipelineBuilder> CreatePipelineBuilder(
    const SharedBasicContext& context,
    string&& pipeline_name,
    const PerVertexBuffer& vertex_buffer,
    const VkDescriptorSetLayout& descriptor_layout,
    bool enable_color_blend) {
  auto pipeline_builder = absl::make_unique<PipelineBuilder>(context);

  (*pipeline_builder)
      .SetName(std::move(pipeline_name))
      .AddVertexInput(kVertexBufferBindingPoint,
                      pipeline::GetPerVertexBindingDescription<Vertex2D>(),
                      vertex_buffer.GetAttributes(/*start_location=*/0))
      .SetPipelineLayout({descriptor_layout}, /*push_constant_ranges=*/{})
      .SetColorBlend({pipeline::GetColorBlendState(enable_color_blend)})
      .SetFrontFaceDirection(/*counter_clockwise=*/false)
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("char.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("char.frag"));

  return pipeline_builder;
}

// Returns a pipeline that renders to 'target_image'.
std::unique_ptr<Pipeline> BuildPipeline(const Image& target_image,
                                        const VkRenderPass& render_pass,
                                        PipelineBuilder* pipeline_builder) {
  return (*pipeline_builder)
      .SetViewport(pipeline::GetFullFrameViewport(target_image.extent()))
      .SetRenderPass(render_pass, kTextSubpassIndex)
      .Build();
}

// Returns texture sampler config for rendering texts.
const SamplableImage::Config& GetTextSamplerConfig() {
  static const SamplableImage::Config* config = nullptr;
  if (config == nullptr) {
    config = new SamplableImage::Config{
        VK_FILTER_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };
  }
  return *config;
}

// Flips Y coordinates of each vertex in NDC.
inline void FlipYCoord(vector<Vertex2D>* vertices) {
  for (auto& vertex : *vertices) {
    vertex.pos.y *= -1;
  }
}

// Returns pos in NDC given 2D coordinate in range [0.0, 1.0].
inline glm::vec2 NormalizePos(const glm::vec2& coordinate) {
  return coordinate * 2.0f - 1.0f;
}

} /* namespace */

CharLoader::CharLoader(const SharedBasicContext& context,
                       const vector<string>& texts,
                       Font font, int font_height) {
  CharImageMap char_image_map;
  {
    const common::CharLib char_lib{texts, GetFontPath(font), font_height};
    const int interval_between_chars = GetIntervalBetweenChars(char_lib);
    char_lib_image_ = absl::make_unique<OffscreenImage>(
        context, kSingleChannel,
        GetCharLibImageExtent(char_lib, interval_between_chars),
        GetTextSamplerConfig());
    space_advance_x_ = GetSpaceAdvanceX(char_lib, *char_lib_image_);
    CreateCharTextures(context, char_lib, interval_between_chars,
                       *char_lib_image_, &char_image_map,
                       &char_texture_info_map_);
  }

  vector<char> char_merge_order;
  char_merge_order.reserve(char_texture_info_map_.size());
  for (const auto& pair : char_texture_info_map_) {
    char_merge_order.emplace_back(pair.first);
  }

  const auto vertex_buffer = CreateVertexBuffer(context, char_merge_order);
  const auto descriptor =
      absl::make_unique<DynamicDescriptor>(context, CreateDescriptorInfos());

  auto render_pass_builder = CreateRenderPassBuilder(context);
  const auto render_pass = BuildRenderPass(*char_lib_image_,
                                           render_pass_builder.get());

  auto pipeline_builder =
      CreatePipelineBuilder(context, "char loader", *vertex_buffer,
                            descriptor->layout(), /*enable_color_blend=*/false);
  const auto pipeline = BuildPipeline(*char_lib_image_, **render_pass,
                                      pipeline_builder.get());

  const vector<RenderPass::RenderOp> render_ops{
      [&](const VkCommandBuffer& command_buffer) {
        pipeline->Bind(command_buffer);
        for (int i = 0; i < char_merge_order.size(); ++i) {
          const auto& char_image =
              char_image_map.find(char_merge_order[i])->second;
          descriptor->PushImageInfos(
              command_buffer, pipeline->layout(),
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              /*image_info_map=*/{{
                  kImageBindingPoint,
                  {char_image->GetDescriptorInfo()}},
              });
          vertex_buffer->Draw(command_buffer, kVertexBufferBindingPoint,
                              /*mesh_index=*/i, /*instance_count=*/1);
        }
      },
  };

  const OneTimeCommand command{context, &context->queues().graphics_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    render_pass->Run(command_buffer, /*framebuffer_index=*/0, render_ops);
  });
}

VkExtent2D CharLoader::GetCharLibImageExtent(const common::CharLib& char_lib,
                                             int interval_between_chars) const {
  ASSERT_NON_EMPTY(char_lib.char_info_map(), "No character loaded");
  int total_width = 0, height = 0;
  for (const auto& pair : char_lib.char_info_map()) {
    if (pair.first != ' ') {
      total_width += pair.second.image->width + interval_between_chars;
      height = std::max(height, pair.second.image->height);
    }
  }
  total_width -= interval_between_chars;
  return VkExtent2D{
      static_cast<uint32_t>(total_width),
      static_cast<uint32_t>(height),
  };
}

absl::optional<float> CharLoader::GetSpaceAdvanceX(
    const common::CharLib& char_lib, const Image& target_image) const {
  const auto found = char_lib.char_info_map().find(' ');
  absl::optional<float> space_advance;
  if (found != char_lib.char_info_map().end()) {
    space_advance = static_cast<float>(found->second.advance.x) /
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

  float offset_x = 0.0f;
  for (const auto& pair : char_lib.char_info_map()) {
    const char character = pair.first;
    if (character == ' ') {
      continue;
    }

    const auto& char_info = pair.second;
    const auto advance_x = static_cast<float>(char_info.advance.x) * ratio.x;
    const glm::vec2 size =
        glm::vec2{char_info.image->width, char_info.image->height} * ratio;
    const glm::vec2 bearing = glm::vec2{char_info.bearing} * ratio;
    char_texture_info_map->emplace(
        character,
        CharTextureInfo{size, bearing, offset_x, advance_x}
    );
    char_image_map->emplace(
        character,
        absl::make_unique<TextureImage>(
            context, /*generate_mipmaps=*/false, GetTextSamplerConfig(),
            TextureBuffer::Info{
                {char_info.image->data},
                VK_FORMAT_R8_UNORM,
                static_cast<uint32_t>(char_info.image->width),
                static_cast<uint32_t>(char_info.image->height),
                kSingleChannel,
            }
        )
    );
    offset_x += size.x + normalized_interval;
  }
}

std::unique_ptr<StaticPerVertexBuffer> CharLoader::CreateVertexBuffer(
    const SharedBasicContext& context,
    const vector<char>& char_merge_order) const {
  vector<Vertex2D> vertices;
  vertices.reserve(text_util::kNumVerticesPerRect * char_merge_order.size());
  for (auto character : char_merge_order) {
    const auto& texture_info = char_texture_info_map_.find(character)->second;
    text_util::AppendCharPosAndTexCoord(
        /*pos_bottom_left=*/
        {texture_info.offset_x, 0.0f},
        /*pos_increment=*/texture_info.size,
        /*tex_coord_bottom_left=*/glm::vec2{0.0f},
        /*tex_coord_increment=*/glm::vec2{1.0f},
        &vertices);
  }
  // The resulting image should be flipped, so that when we use it later, we
  // don't have to flip Y coordinates again.
  FlipYCoord(&vertices);

  return absl::make_unique<StaticPerVertexBuffer>(
      context, PerVertexBuffer::ShareIndicesDataInfo{
          /*num_meshes=*/static_cast<int>(char_merge_order.size()),
          /*per_mesh_vertices=*/
          {vertices, /*num_units_per_mesh=*/text_util::kNumVerticesPerRect},
          /*shared_indices=*/
          {PerVertexBuffer::VertexDataInfo{text_util::GetIndicesPerRect()}},
      },
      pipeline::GetVertexAttribute<Vertex2D>()
  );
}

TextLoader::TextLoader(const SharedBasicContext& context,
                       const vector<string>& texts,
                       CharLoader::Font font, int font_height) {
  const auto& longest_text = std::max_element(
      texts.begin(), texts.end(), [](const string& lhs, const string& rhs) {
        return lhs.length() > rhs.length();
      });
  DynamicPerVertexBuffer vertex_buffer{
      context, text_util::GetVertexDataSize(longest_text->length()),
      pipeline::GetVertexAttribute<Vertex2D>()};

  auto descriptor = absl::make_unique<StaticDescriptor>(
      context, CreateDescriptorInfos());
  auto render_pass_builder = CreateRenderPassBuilder(context);
  // Advance can be negative, and thus bounding boxes of characters may have
  // overlap, hence we need to enable color blending.
  auto pipeline_builder =
      CreatePipelineBuilder(context, "text loader", vertex_buffer,
                            descriptor->layout(), /*enable_color_blend=*/true);

  const CharLoader char_loader{context, texts, font, font_height};
  text_texture_infos_.reserve(texts.size());
  for (const auto& text : texts) {
    text_texture_infos_.emplace_back(
        CreateTextTexture(context, text, font_height, char_loader,
                          descriptor.get(), render_pass_builder.get(),
                          pipeline_builder.get(), &vertex_buffer));
  }
}

TextLoader::TextTextureInfo TextLoader::CreateTextTexture(
    const SharedBasicContext& context,
    const string& text, int font_height,
    const CharLoader& char_loader,
    StaticDescriptor* descriptor,
    NaiveRenderPassBuilder* render_pass_builder,
    PipelineBuilder* pipeline_builder,
    DynamicPerVertexBuffer* vertex_buffer) const {
  float total_advance_x = 0.0f;
  float highest_base_y = 0.0f;
  for (auto character : text) {
    if (character == ' ') {
      total_advance_x += char_loader.space_advance();
    } else {
      const auto& texture_info = char_loader.char_texture_info(character);
      total_advance_x += texture_info.advance_x;
      highest_base_y = std::max(highest_base_y,
                                texture_info.size.y - texture_info.bearing.y);
    }
  }

  // In the coordinate of character library image, the width of 'text' is
  // 'total_advance_x' and the height is 1.0. Note that the character library
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
  auto text_image = absl::make_unique<OffscreenImage>(
      context, kSingleChannel, text_image_extent, GetTextSamplerConfig());

  // The resulting image should be flipped, so that when we use it later, we
  // don't have to flip Y coordinates again.
  vector<Vertex2D> vertices;
  text_util::LoadCharsVertexData(text, char_loader, ratio,
                                 /*initial_offset_x=*/0.0f, base_y,
                                 /*flip_y=*/true, &vertices);
  vertex_buffer->CopyHostData(PerVertexBuffer::ShareIndicesDataInfo{
      /*num_meshes=*/static_cast<int>(text.length()),
      /*per_mesh_vertices=*/
      {vertices, /*num_units_per_mesh=*/text_util::kNumVerticesPerRect},
      /*shared_indices=*/
      {PerVertexBuffer::VertexDataInfo{text_util::GetIndicesPerRect()}},
  });

  descriptor->UpdateImageInfos(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      /*image_info_map=*/{
          {kImageBindingPoint,
           {char_loader.library_image()->GetDescriptorInfo()}},
  });
  const auto render_pass = BuildRenderPass(*text_image, render_pass_builder);
  const auto pipeline = BuildPipeline(*text_image, **render_pass,
                                      pipeline_builder);

  const vector<RenderPass::RenderOp> render_ops{
      [&](const VkCommandBuffer& command_buffer) {
        pipeline->Bind(command_buffer);
        descriptor->Bind(command_buffer, pipeline->layout());
        for (int i = 0; i < text.length(); ++i) {
          vertex_buffer->Draw(command_buffer, kVertexBufferBindingPoint,
                              /*mesh_index=*/i, /*instance_count=*/1);
        }
      },
  };

  const OneTimeCommand command{context, &context->queues().graphics_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    render_pass->Run(command_buffer, /*framebuffer_index=*/0, render_ops);
  });

  return TextTextureInfo{util::GetAspectRatio(text_image_extent), base_y,
                         std::move(text_image)};
}

namespace text_util {

const array<uint32_t, kNumIndicesPerRect>& GetIndicesPerRect() {
  static const array<uint32_t, kNumIndicesPerRect>* indices_per_rect = nullptr;
  if (indices_per_rect == nullptr) {
    indices_per_rect = new array<uint32_t, kNumIndicesPerRect>{
        0, 1, 2, 0, 2, 3,
    };
  }
  return *indices_per_rect;
}

void AppendCharPosAndTexCoord(const glm::vec2& pos_bottom_left,
                              const glm::vec2& pos_increment,
                              const glm::vec2& tex_coord_bottom_left,
                              const glm::vec2& tex_coord_increment,
                              vector<Vertex2D>* vertices) {
  const glm::vec2 pos_top_right = pos_bottom_left + pos_increment;
  const glm::vec2 tex_coord_top_right = tex_coord_bottom_left +
                                        tex_coord_increment;
  vertices->reserve(vertices->size() + kNumVerticesPerRect);
  vertices->emplace_back(Vertex2D{
      NormalizePos(pos_bottom_left),
      tex_coord_bottom_left,
  });
  vertices->emplace_back(Vertex2D{
      NormalizePos({pos_top_right.x, pos_bottom_left.y}),
      {tex_coord_top_right.x, tex_coord_bottom_left.y},
  });
  vertices->emplace_back(Vertex2D{
      NormalizePos(pos_top_right),
      tex_coord_top_right,
  });
  vertices->emplace_back(Vertex2D{
      NormalizePos({pos_bottom_left.x, pos_top_right.y}),
      {tex_coord_bottom_left.x, tex_coord_top_right.y},
  });
  // If the height of character is negative, we reverse the vertices order so
  // that the faces they form don't get culled.
  if (pos_increment.y < 0) {
    std::reverse(vertices->end() - kNumVerticesPerRect, vertices->end());
  }
}

float LoadCharsVertexData(const string& text, const CharLoader& char_loader,
                          const glm::vec2& ratio, float initial_offset_x,
                          float base_y, bool flip_y,
                          vector<Vertex2D>* vertices) {
  float offset_x = initial_offset_x;
  vertices->reserve(
      vertices->size() + text_util::kNumVerticesPerRect * text.length());
  for (auto character : text) {
    if (character == ' ') {
      offset_x += char_loader.space_advance() * ratio.x;
      continue;
    }
    const auto& texture_info = char_loader.char_texture_info(character);
    const glm::vec2& size_in_tex = texture_info.size;
    text_util::AppendCharPosAndTexCoord(
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
  if (flip_y) {
    FlipYCoord(vertices);
  }

  return offset_x;
}

} /* namespace text_util */

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
