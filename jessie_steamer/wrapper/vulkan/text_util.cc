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
#include "third_party/absl/memory/memory.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using common::Vertex2D;
using std::array;
using std::string;
using std::vector;

constexpr int kColorAttachmentIndex = 0;
constexpr int kImageBindingPoint = 0;
constexpr int kNativeSubpassIndex = 0;
constexpr int kSingleChannel = 1;

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
int GetIntervalBetweenChars(int font_height) {
  constexpr int kFontHeightToIntervalRatio = 20;
  constexpr int kMinIntervalBetweenChars = 2;
  constexpr int kMaxIntervalBetweenChars = 20;
  int interval = font_height / kFontHeightToIntervalRatio;
  interval = std::min(interval, kMaxIntervalBetweenChars);
  interval = std::max(interval, kMinIntervalBetweenChars);
  return interval;
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
std::unique_ptr<RenderPassBuilder> CreateRenderPassBuilder(
    const SharedBasicContext& context) {
  auto render_pass_builder = absl::make_unique<RenderPassBuilder>(context);

  (*render_pass_builder)
      .SetNumFramebuffers(1)
      .SetAttachment(
          kColorAttachmentIndex,
          RenderPassBuilder::Attachment{
              /*attachment_ops=*/RenderPassBuilder::Attachment::ColorOps{
                  /*load_color_op=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                  /*store_color_op=*/VK_ATTACHMENT_STORE_OP_STORE,
              },
              /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
              /*final_layout=*/VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          }
      )
      .SetSubpass(
          kNativeSubpassIndex,
          /*color_refs=*/{
              VkAttachmentReference{
                  kColorAttachmentIndex,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              },
          },
          /*depth_stencil_ref=*/absl::nullopt
      )
      .AddSubpassDependency(RenderPassBuilder::SubpassDependency{
          /*prev_subpass=*/RenderPassBuilder::SubpassDependency::SubpassInfo{
              kExternalSubpassIndex,
              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              /*access_mask=*/0,
          },
          /*next_subpass=*/RenderPassBuilder::SubpassDependency::SubpassInfo{
              kNativeSubpassIndex,
              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          },
      });

  return render_pass_builder;
}

// Returns a render pass that renders to 'target_image'.
std::unique_ptr<RenderPass> BuildRenderPass(
    const Image& target_image, RenderPassBuilder* render_pass_builder) {
  return (*render_pass_builder)
      .UpdateAttachmentImage(
          kColorAttachmentIndex,
          [&target_image](int framebuffer_index) -> const Image& {
            return target_image;
          })
      .Build();
}

// Returns a pipeline builder, assuming the per-vertex data is of type Vertex2D,
// and the front face direction is clockwise, since we will flip Y coordinates.
std::unique_ptr<PipelineBuilder> CreatePipelineBuilder(
    const SharedBasicContext& context,
    const VkDescriptorSetLayout& descriptor_layout,
    bool enable_color_blend) {
  auto pipeline_builder = absl::make_unique<PipelineBuilder>(context);

  (*pipeline_builder)
      .SetVertexInput(
          pipeline::GetBindingDescriptions(
              {pipeline::GetPerVertexBinding<Vertex2D>()}),
          pipeline::GetAttributeDescriptions(
              {pipeline::GetPerVertexAttribute<Vertex2D>()}))
      .SetPipelineLayout({descriptor_layout}, /*push_constant_ranges=*/{})
      .SetColorBlend({pipeline::GetColorBlendState(enable_color_blend)})
      .SetFrontFaceDirection(/*counter_clockwise=*/false);

  return pipeline_builder;
}

// Returns a pipeline that renders to 'target_image'.
std::unique_ptr<Pipeline> BuildPipeline(const Image& target_image,
                                        const VkRenderPass& render_pass,
                                        PipelineBuilder* pipeline_builder) {
  using common::file::GetShaderPath;
  return (*pipeline_builder)
      .SetViewport(
          /*viewport=*/VkViewport{
              /*x=*/0.0f,
              /*y=*/0.0f,
              static_cast<float>(target_image.extent().width),
              static_cast<float>(target_image.extent().height),
              /*minDepth=*/0.0f,
              /*maxDepth=*/1.0f,
          },
          /*scissor=*/VkRect2D{
              /*offset=*/{0, 0},
              target_image.extent(),
          }
      )
      .SetRenderPass(render_pass, kNativeSubpassIndex)
      .AddShader(VK_SHADER_STAGE_VERTEX_BIT,
                 GetShaderPath("vulkan/char.vert.spv"))
      .AddShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 GetShaderPath("vulkan/char.frag.spv"))
      .Build();
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

CharLoader::CharLoader(SharedBasicContext context,
                       const vector<string>& texts,
                       Font font, int font_height)
    : context_{std::move(context)} {
  CharImageMap char_image_map;
  {
    const common::CharLib char_lib{texts, GetFontPath(font), font_height};
    const int interval_between_chars = GetIntervalBetweenChars(font_height);
    char_lib_image_ = absl::make_unique<OffscreenImage>(
        context_, kSingleChannel,
        GetCharLibImageExtent(char_lib, interval_between_chars, font_height));
    space_advance_x_ = GetSpaceAdvanceX(char_lib, *char_lib_image_);
    CreateCharTextures(char_lib, interval_between_chars, *char_lib_image_,
                       &char_image_map, &char_texture_info_map_);
  }

  vector<char> char_merge_order;
  char_merge_order.reserve(char_texture_info_map_.size());
  for (const auto& pair : char_texture_info_map_) {
    char_merge_order.emplace_back(pair.first);
  }

  const auto vertex_buffer = CreateVertexBuffer(char_merge_order);
  const auto descriptor =
      absl::make_unique<DynamicDescriptor>(context_, CreateDescriptorInfos());

  auto render_pass_builder = CreateRenderPassBuilder(context_);
  const auto render_pass = BuildRenderPass(*char_lib_image_,
                                           render_pass_builder.get());

  auto pipeline_builder = CreatePipelineBuilder(context_, descriptor->layout(),
                                                /*enable_color_blend=*/false);
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
          vertex_buffer->Draw(command_buffer, /*mesh_index=*/i,
                              /*instance_count=*/1);
        }
      },
  };

  const OneTimeCommand command{context_, &context_->queues().graphics_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    render_pass->Run(command_buffer, /*framebuffer_index=*/0, render_ops);
  });
}

VkExtent2D CharLoader::GetCharLibImageExtent(const common::CharLib& char_lib,
                                             int interval_between_chars,
                                             int font_height) const {
  ASSERT_NON_EMPTY(char_lib.char_info_map(), "No character loaded");
  int total_width = 0;
  for (const auto& pair : char_lib.char_info_map()) {
    if (pair.first != ' ') {
      total_width += pair.second.image->width + interval_between_chars;
    }
  }
  total_width -= interval_between_chars;
  return VkExtent2D{
      static_cast<uint32_t>(total_width),
      static_cast<uint32_t>(font_height),
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
    const float advance_x = static_cast<float>(char_info.advance.x) * ratio.x;
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
            context_,
            /*generate_mipmaps=*/false,
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
      context_, PerVertexBuffer::ShareIndicesDataInfo{
          /*num_meshes=*/static_cast<int>(char_merge_order.size()),
          /*per_mesh_vertices=*/
          {vertices, /*num_units_per_mesh=*/text_util::kNumVerticesPerRect},
          /*shared_indices=*/
          {PerVertexBuffer::VertexDataInfo{text_util::GetIndicesPerRect()}},
      }
  );
}

TextLoader::TextLoader(SharedBasicContext context,
                       const vector<string>& texts,
                       CharLoader::Font font, int font_height)
    : context_{std::move(context)} {
  const auto& longest_text = std::max_element(
      texts.begin(), texts.end(), [](const string& lhs, const string& rhs) {
        return lhs.length() > rhs.length();
      });
  DynamicPerVertexBuffer vertex_buffer{
    context_, text_util::GetVertexDataSize(longest_text->length())};

  auto descriptor = absl::make_unique<StaticDescriptor>(
      context_, CreateDescriptorInfos());
  auto render_pass_builder = CreateRenderPassBuilder(context_);
  // Advance can be negative, and thus bounding boxes of characters may have
  // overlap, hence we need to enable color blending.
  auto pipeline_builder = CreatePipelineBuilder(
      context_, descriptor->layout(), /*enable_color_blend=*/true);

  const CharLoader char_loader{context_, texts, font, font_height};
  text_texture_infos_.reserve(texts.size());
  for (const auto& text : texts) {
    text_texture_infos_.emplace_back(
        CreateTextTexture(text, font_height, char_loader, descriptor.get(),
                          render_pass_builder.get(), pipeline_builder.get(),
                          &vertex_buffer));
  }
}

TextLoader::TextTextureInfo TextLoader::CreateTextTexture(
    const string& text, int font_height,
    const CharLoader& char_loader,
    StaticDescriptor* descriptor,
    RenderPassBuilder* render_pass_builder,
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
  // should also consider its width height ratio. The height of text texture
  // will be made 'font_height'.
  const glm::vec2 ratio = 1.0f / glm::vec2{total_advance_x, 1.0f};
  const VkExtent2D text_image_extent{
      static_cast<uint32_t>(
          (total_advance_x * char_loader.GetWidthHeightRatio()) *
          (static_cast<float>(font_height) / 1.0f)),
      static_cast<uint32_t>(font_height),
  };
  const float base_y = highest_base_y;
  auto text_image = absl::make_unique<OffscreenImage>(context_, kSingleChannel,
                                                      text_image_extent);

  // The resulting image should be flipped, so that when we use it later, we
  // don't have to flip Y coordinates again.
  text_util::LoadCharsVertexData(text, char_loader, ratio,
                                 /*initial_offset_x=*/0.0f, base_y,
                                 /*flip_y=*/true, vertex_buffer);

  descriptor->UpdateImageInfos(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, {
      {kImageBindingPoint, {char_loader.library_image()->GetDescriptorInfo()}},
  });
  const auto render_pass = BuildRenderPass(*text_image, render_pass_builder);
  const auto pipeline = BuildPipeline(*text_image, **render_pass,
                                      pipeline_builder);

  const vector<RenderPass::RenderOp> render_ops{
      [&](const VkCommandBuffer& command_buffer) {
        pipeline->Bind(command_buffer);
        descriptor->Bind(command_buffer, pipeline->layout());
        for (int i = 0; i < text.length(); ++i) {
          vertex_buffer->Draw(command_buffer, /*mesh_index=*/i,
                              /*instance_count=*/1);
        }
      },
  };

  const OneTimeCommand command{context_, &context_->queues().graphics_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    render_pass->Run(command_buffer, /*framebuffer_index=*/0, render_ops);
  });

  return TextTextureInfo{
      util::GetWidthHeightRatio(text_image_extent), base_y,
      std::move(text_image),
  };
}

namespace text_util {

const array<uint32_t, kNumIndicesPerRect>& GetIndicesPerRect() {
  static array<uint32_t, kNumIndicesPerRect>* indices_per_rect = nullptr;
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
}

float LoadCharsVertexData(const string& text, const CharLoader& char_loader,
                          const glm::vec2& ratio, float initial_offset_x,
                          float base_y, bool flip_y,
                          DynamicPerVertexBuffer* vertex_buffer) {
  float offset_x = initial_offset_x;
  vector<Vertex2D> vertices;
  vertices.reserve(text_util::kNumVerticesPerRect * text.length());
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
        &vertices);
    offset_x += texture_info.advance_x * ratio.x;
  }
  if (flip_y) {
    FlipYCoord(&vertices);
  }

  vertex_buffer->CopyHostData(PerVertexBuffer::ShareIndicesDataInfo{
      /*num_meshes=*/static_cast<int>(text.length()),
      /*per_mesh_vertices=*/
      {vertices, /*num_units_per_mesh=*/text_util::kNumVerticesPerRect},
      /*shared_indices=*/
      {PerVertexBuffer::VertexDataInfo{text_util::GetIndicesPerRect()}},
  });

  return offset_x;
}

} /* namespace text_util */

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
