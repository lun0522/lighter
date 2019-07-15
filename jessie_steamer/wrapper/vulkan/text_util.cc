//
//  text_util.cc
//
//  Created by Pujun Lun on 6/20/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/text_util.h"

#include <algorithm>

#include "absl/memory/memory.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/vertex_input_util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::array;
using std::string;
using std::vector;

using common::VertexAttrib2D;

constexpr int kImageBindingPoint = 0;

string GetFontPath(CharLoader::Font font) {
  const string prefix = "external/resource/font/";
  switch (font) {
    case CharLoader::Font::kGeorgia:
      return prefix + "georgia.ttf";
    case CharLoader::Font::kOstrich:
      return prefix + "ostrich.ttf";
  }
}

int GetIntervalBetweenChars(int font_height) {
  constexpr int kFontHeightToIntervalRatio = 20;
  constexpr int kMinIntervalBetweenChars = 2;
  constexpr int kMaxIntervalBetweenChars = 20;
  int interval = font_height / kFontHeightToIntervalRatio;
  interval = std::min(interval, kMaxIntervalBetweenChars);
  interval = std::max(interval, kMinIntervalBetweenChars);
  return interval;
}

vector<Descriptor::Info> CreateDescriptorInfos() {
  return {
      Descriptor::Info{
          /*descriptor_type=*/VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
          /*bindings=*/{
              Descriptor::Info::Binding{
                  Descriptor::ResourceType::kTextureDiffuse,
                  kImageBindingPoint,
                  /*array_length=*/1,
              },
          },
      },
  };
}

std::unique_ptr<RenderPassBuilder> CreateRenderPassBuilder(
    const SharedBasicContext& context) {
  auto render_pass_builder = absl::make_unique<RenderPassBuilder>(context);

  (*render_pass_builder)
      .set_num_framebuffer(1)
      .set_subpass_description(
          /*index=*/0,
          RenderPassBuilder::SubpassAttachments{
              /*color_refs=*/{
                  VkAttachmentReference{
                      /*attachment=*/0,
                      /*layout=*/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  },
              },
              /*depth_stencil_ref=*/absl::nullopt,
          }
      )
      .add_subpass_dependency(RenderPassBuilder::SubpassDependency{
          /*src_info=*/RenderPassBuilder::SubpassDependency::SubpassInfo{
              /*index=*/VK_SUBPASS_EXTERNAL,
              /*stage_mask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              /*access_mask=*/0,
          },
          /*dst_info=*/RenderPassBuilder::SubpassDependency::SubpassInfo{
              /*index=*/0,
              /*stage_mask=*/VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              /*access_mask=*/VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          },
      });

  return render_pass_builder;
}

std::unique_ptr<RenderPass> BuildRenderPass(
    VkExtent2D target_extent,
    RenderPassBuilder::GetImage&& get_target_image,
    RenderPassBuilder* render_pass_builder) {
  return (*render_pass_builder)
      .set_framebuffer_size(target_extent)
      .set_attachment(
          /*index=*/0,
          RenderPassBuilder::Attachment{
              /*attachment_ops=*/RenderPassBuilder::Attachment::ColorOps{
                  /*load_color=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                  /*store_color=*/VK_ATTACHMENT_STORE_OP_STORE,
              },
              /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
              /*final_layout=*/VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          },
          std::move(get_target_image)
      )
      .Build();
}

std::unique_ptr<PipelineBuilder> CreatePipelineBuilder(
    const SharedBasicContext& context,
    const VkDescriptorSetLayout& descriptor_layout,
    bool enable_alpha_blend) {
  auto pipeline_builder = absl::make_unique<PipelineBuilder>(context);

  (*pipeline_builder)
      .set_vertex_input(
          GetBindingDescriptions({GetPerVertexBindings<VertexAttrib2D>()}),
          GetAttributeDescriptions({GetVertexAttributes<VertexAttrib2D>()}))
      .set_layout({descriptor_layout}, /*push_constant_ranges=*/{})
      .enable_alpha_blend()
      .set_front_face_clockwise();  // since we will flip y coordinates
  if (enable_alpha_blend) {
    pipeline_builder->enable_alpha_blend();
  }

  return pipeline_builder;
}

std::unique_ptr<Pipeline> BuildPipeline(VkExtent2D target_extent,
                                        const VkRenderPass& render_pass,
                                        PipelineBuilder* pipeline_builder) {
  return (*pipeline_builder)
      .set_viewport({
          /*viewport=*/VkViewport{
              /*x=*/0.0f,
              /*y=*/0.0f,
              static_cast<float>(target_extent.width),
              static_cast<float>(target_extent.height),
              /*minDepth=*/0.0f,
              /*maxDepth=*/1.0f,
          },
          /*scissor=*/VkRect2D{
              /*offset=*/{0, 0},
              target_extent,
          },
      })
      .set_render_pass(render_pass, /*subpass_index=*/0)
      .add_shader({VK_SHADER_STAGE_VERTEX_BIT,
                   "jessie_steamer/shader/vulkan/simple_2d.vert.spv"})
      .add_shader({VK_SHADER_STAGE_FRAGMENT_BIT,
                   "jessie_steamer/shader/vulkan/simple_2d.frag.spv"})
      .Build();
}

// Flips y coordinate of each vertex in NDC.
inline void FlipY(vector<VertexAttrib2D>* vertices) {
  for (auto& vertex : *vertices) {
    vertex.pos.y *= -1;
  }
}

std::unique_ptr<PerVertexBuffer> CreateVertexBuffer(
    const SharedBasicContext& context,
    const vector<char>& char_merge_order,
    const absl::flat_hash_map<char, CharLoader::CharTextureInfo>&
        char_texture_map) {
  vector<VertexAttrib2D> vertices;
  vertices.reserve(text_util::kNumVerticesPerRect * char_merge_order.size());
  for (auto c : char_merge_order) {
    const CharLoader::CharTextureInfo& texture_info =
        char_texture_map.find(c)->second;
    text_util::AppendCharPosAndTexCoord(
        /*pos_bottom_left=*/
        {texture_info.offset_x, 0.0f},
        /*pos_increment=*/texture_info.size,
        /*tex_coord_bottom_left=*/glm::vec2{0.0f},
        /*tex_coord_increment=*/glm::vec2{1.0f},
        &vertices);
  }
  // resulting image should be flipped, so that when we use it later, we don't
  // have to flip y coordinates again
  FlipY(&vertices);

  std::unique_ptr<PerVertexBuffer> buffer =
      absl::make_unique<PerVertexBuffer>(context);
  buffer->Init(PerVertexBuffer::InfoReuse{
      /*num_mesh=*/static_cast<int>(char_merge_order.size()),
      /*per_mesh_vertices=*/
      {vertices, /*unit_count=*/text_util::kNumVerticesPerRect},
      /*shared_indices=*/{text_util::indices_per_rect()},
  });
  return buffer;
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
  common::CharLib char_lib{texts, GetFontPath(font), font_height};
  if (char_lib.char_info_map().empty()) {
    FATAL("No character loaded");
  }

  CharTextures char_textures = CreateCharTextures(char_lib, font_height);
  image_ = absl::make_unique<OffscreenImage>(context_, /*channel=*/1,
                                             char_textures.extent_after_merge);
  width_height_ratio_ = GetWidthHeightRatio(char_textures.extent_after_merge);

  vector<char> char_merge_order;
  char_merge_order.reserve(char_texture_map_.size());
  for (const auto& ch : char_texture_map_) {
    char_merge_order.emplace_back(ch.first);
  }

  auto vertex_buffer = CreateVertexBuffer(context_, char_merge_order,
                                          char_texture_map_);

  auto descriptor = absl::make_unique<DynamicDescriptor>(
      context_, CreateDescriptorInfos());

  auto render_pass_builder = CreateRenderPassBuilder(context_);
  auto render_pass = BuildRenderPass(
      char_textures.extent_after_merge,
      [this](int index) -> const Image& { return *image_; },
      render_pass_builder.get());

  auto pipeline_builder = CreatePipelineBuilder(context_, descriptor->layout(),
                                                /*enable_alpha_blend=*/false);
  auto pipeline = BuildPipeline(char_textures.extent_after_merge,
                                **render_pass, pipeline_builder.get());

  vector<RenderPass::RenderOp> render_ops{
      [&](const VkCommandBuffer& command_buffer) {
        pipeline->Bind(command_buffer);
        for (int i = 0; i < char_merge_order.size(); ++i) {
          descriptor->PushImageInfos(
              command_buffer, pipeline->layout(),
              /*descriptor_type=*/VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              /*image_infos=*/{
                  {kImageBindingPoint,
                   {char_textures.char_image_map[char_merge_order[i]]
                        ->descriptor_info()}},
              });
          vertex_buffer->Draw(command_buffer, /*mesh_index=*/i,
              /*instance_count=*/1);
        }
      },
  };

  OneTimeCommand command{context_, &context_->queues().graphics};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    render_pass->Run(command_buffer, /*framebuffer_index=*/0, render_ops);
  });
}

CharLoader::CharTextures CharLoader::CreateCharTextures(
    const common::CharLib& char_lib, int font_height) {
  CharTextures char_textures;

  const int interval_between_chars = GetIntervalBetweenChars(font_height);
  int total_width = 0;
  for (const auto& ch : char_lib.char_info_map()) {
    if (ch.first != ' ') {
      total_width += ch.second.image->width + interval_between_chars;
    }
  }
  total_width -= interval_between_chars;
  char_textures.extent_after_merge = VkExtent2D{
      static_cast<uint32_t>(total_width),
      static_cast<uint32_t>(font_height),
  };
  const glm::vec2 ratio = 1.0f / glm::vec2{total_width, font_height};

  const float interval_in_tex = interval_between_chars * ratio.x;
  float offset_x = 0.0f;
  for (const auto& ch : char_lib.char_info_map()) {
    const char character = ch.first;
    const auto& info = ch.second;
    const float advance_x = info.advance_x * ratio.x;
    if (character == ' ') {
      // no texture will be wasted for space. we only record its advance
      space_advance_x_ = advance_x;
    } else {
      const glm::vec2 size =
          glm::vec2{info.image->width, info.image->height} * ratio;
      const glm::vec2 bearing = glm::vec2{info.bearing} * ratio;
      char_texture_map_.emplace(
          character,
          CharTextureInfo{size, bearing, offset_x, advance_x}
      );
      char_textures.char_image_map.emplace(
          character,
          absl::make_unique<TextureImage>(
              context_,
              /*generate_mipmaps=*/false,
              TextureBuffer::Info{
                  {info.image->data},
                  VK_FORMAT_R8_UNORM,
                  static_cast<uint32_t>(info.image->width),
                  static_cast<uint32_t>(info.image->height),
                  /*channel=*/1,
              }
          )
      );
      offset_x += size.x + interval_in_tex;
    }
  }
  return char_textures;
}

TextLoader::TextLoader(SharedBasicContext context,
                       const vector<string>& texts,
                       CharLoader::Font font, int font_height)
    : context_{std::move(context)} {
  DynamicPerVertexBuffer vertex_buffer{context_};
  const auto& longest_text = std::max_element(
      texts.begin(), texts.end(), [](const string& lhs, const string& rhs) {
        return lhs.length() > rhs.length();
      });
  vertex_buffer.Reserve(text_util::GetVertexDataSize(longest_text->length()));

  auto descriptor = absl::make_unique<StaticDescriptor>(
      context_, CreateDescriptorInfos());
  auto render_pass_builder = CreateRenderPassBuilder(context_);
  auto pipeline_builder = CreatePipelineBuilder(context_, descriptor->layout(),
                                                /*enable_alpha_blend=*/true);

  CharLoader char_loader{context_, texts, font, font_height};
  text_textures_.reserve(texts.size());
  for (const auto& text : texts) {
    text_textures_.emplace_back(
        CreateTextTexture(text, font_height, char_loader, descriptor.get(),
                          render_pass_builder.get(), pipeline_builder.get(),
                          &vertex_buffer));
  }
}

TextLoader::TextTexture TextLoader::CreateTextTexture(
    const string& text, int font_height,
    const CharLoader& char_loader,
    StaticDescriptor* descriptor,
    RenderPassBuilder* render_pass_builder,
    PipelineBuilder* pipeline_builder,
    DynamicPerVertexBuffer* vertex_buffer) const {
  const auto& char_texture_map = char_loader.char_texture_map();
  float total_width_in_loader_tex = 0;
  float highest_baseline = 0.0f;
  for (auto c : text) {
    if (c == ' ') {
      total_width_in_loader_tex += char_loader.space_advance();
    } else {
      const auto& char_texture = char_texture_map.find(c)->second;
      total_width_in_loader_tex += char_texture.advance_x;
      highest_baseline = std::max(highest_baseline,
                                  char_texture.size.y - char_texture.bearing.y);
    }
  }
  const glm::vec2 ratio = glm::vec2{1.0f / total_width_in_loader_tex, 1.0f};
  const float base_y = highest_baseline;

  VkExtent2D extent_after_merge{
      static_cast<uint32_t>(total_width_in_loader_tex *
                            char_loader.width_height_ratio() *
                            (font_height / 1.0f)),
      static_cast<uint32_t>(font_height),
  };
  TextTexture text_texture{
      GetWidthHeightRatio(extent_after_merge), base_y,
      absl::make_unique<OffscreenImage>(context_, /*channel=*/1,
                                        extent_after_merge),
  };

  // resulting image should be flipped, so that when we use it later, we don't
  // have to flip y coordinates again
  text_util::LoadCharsVertexData(text, char_loader, ratio,
                                 /*initial_offset_x=*/0.0f, base_y,
                                 /*flip_y=*/true, vertex_buffer);

  descriptor->UpdateImageInfos(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, {
      {kImageBindingPoint, {char_loader.texture()->descriptor_info()}},
  });

  auto render_pass = BuildRenderPass(
      extent_after_merge,
      [&](int index) -> const Image& { return *text_texture.image; },
      render_pass_builder);

  auto pipeline = BuildPipeline(extent_after_merge, **render_pass,
                                pipeline_builder);

  vector<RenderPass::RenderOp> render_ops{
      [&](const VkCommandBuffer& command_buffer) {
        pipeline->Bind(command_buffer);
        descriptor->Bind(command_buffer, pipeline->layout());
        for (int i = 0; i < text.length(); ++i) {
          vertex_buffer->Draw(command_buffer, /*mesh_index=*/i,
                              /*instance_count=*/1);
        }
      },
  };

  OneTimeCommand command{context_, &context_->queues().graphics};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    render_pass->Run(command_buffer, /*framebuffer_index=*/0, render_ops);
  });

  return text_texture;
}

namespace text_util {

const array<uint32_t, kNumIndicesPerRect>& indices_per_rect() {
  static array<uint32_t, kNumIndicesPerRect>* kIndicesPerRect = nullptr;
  if (kIndicesPerRect == nullptr) {
    kIndicesPerRect = new array<uint32_t, kNumIndicesPerRect>{
        0, 1, 2, 0, 2, 3,
    };
  }
  return *kIndicesPerRect;
}

void AppendCharPosAndTexCoord(const glm::vec2 &pos_bottom_left,
                              const glm::vec2 &pos_increment,
                              const glm::vec2 &tex_coord_bottom_left,
                              const glm::vec2 &tex_coord_increment,
                              vector<VertexAttrib2D> *vertices) {
  const glm::vec2 pos_top_right = pos_bottom_left + pos_increment;
  const glm::vec2 tex_coord_top_right = tex_coord_bottom_left +
                                        tex_coord_increment;
  vertices->emplace_back(VertexAttrib2D{
      NormalizePos(pos_bottom_left),
      tex_coord_bottom_left,
  });
  vertices->emplace_back(VertexAttrib2D{
      NormalizePos({pos_top_right.x, pos_bottom_left.y}),
      {tex_coord_top_right.x, tex_coord_bottom_left.y},
  });
  vertices->emplace_back(VertexAttrib2D{
      NormalizePos(pos_top_right),
      tex_coord_top_right,
  });
  vertices->emplace_back(VertexAttrib2D{
      NormalizePos({pos_bottom_left.x, pos_top_right.y}),
      {tex_coord_bottom_left.x, tex_coord_top_right.y},
  });
}

float LoadCharsVertexData(const string& text, const CharLoader& char_loader,
                         const glm::vec2& ratio, float initial_offset_x,
                         float base_y, bool flip_y,
                         DynamicPerVertexBuffer* vertex_buffer) {
  float offset_x = initial_offset_x;
  vector<VertexAttrib2D> vertices;
  vertices.reserve(text_util::kNumVerticesPerRect * text.length());
  for (auto c : text) {
    if (c == ' ') {
      offset_x += char_loader.space_advance() * ratio.x;
      continue;
    }
    const auto& char_texture = char_loader.char_texture_map().find(c)->second;
    const glm::vec2& size_in_tex = char_texture.size;
    text_util::AppendCharPosAndTexCoord(
        /*pos_bottom_left=*/
        {offset_x + char_texture.bearing.x * ratio.x,
         base_y + (char_texture.bearing.y - size_in_tex.y) * ratio.y},
        /*pos_increment=*/size_in_tex * ratio,
        /*tex_coord_bottom_left=*/
        {char_texture.offset_x, 0.0f},
        /*tex_coord_increment=*/size_in_tex,
        &vertices);
    offset_x += char_texture.advance_x * ratio.x;
  }
  if (flip_y) {
    FlipY(&vertices);
  }

  vertex_buffer->Init(PerVertexBuffer::InfoReuse{
      /*num_mesh=*/static_cast<int>(text.length()),
      /*per_mesh_vertices=*/
      {vertices, /*unit_count=*/text_util::kNumVerticesPerRect},
      /*shared_indices=*/{text_util::indices_per_rect()},
  });

  return offset_x;
}

} /* namespace text_util */

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
