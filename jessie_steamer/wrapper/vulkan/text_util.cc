//
//  text_util.cc
//
//  Created by Pujun Lun on 6/20/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/text_util.h"

#include "absl/memory/memory.h"
#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/vertex_input_util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::string;
using std::vector;

using absl::flat_hash_map;
using common::VertexAttrib2D;
using glm::vec2;

string GetFontPath(CharLoader::Font font) {
  const string prefix = "external/resource/font/";
  switch (font) {
    case CharLoader::Font::kGeorgia:
      return prefix + "georgia.ttf";
    case CharLoader::Font::kOstrich:
      return prefix + "ostrich.ttf";
  }
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

  CharTextures char_textures;
  char_texture_map_ = CreateCharTextureMap(char_lib, font_height,
                                           &char_textures);
  image_ = absl::make_unique<OffscreenImage>(context_, /*channel=*/1,
                                             char_textures.extent_after_merge);

  auto render_pass = CreateRenderPass(
      char_textures.extent_after_merge,
      [this](int index) -> const Image& { return *image_; });
  auto descriptor = CreateDescriptor();
  auto pipeline = CreatePipeline(char_textures.extent_after_merge,
                                 *render_pass, *descriptor);
  vector<char> char_merge_order;
  auto vertex_buffer = CreateVertexBuffer(&char_merge_order);

  OneTimeCommand command{context_, &context_->queues().graphics};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    render_pass->Run(
        command_buffer, /*framebuffer_index=*/0,
        [&]() {
          pipeline->Bind(command_buffer);
          for (int i = 0; i < char_merge_order.size(); ++i) {
            const char character = char_merge_order[i];
            if (character == ' ') {
              continue;
            }
            descriptor->UpdateImageInfos(
                command_buffer, pipeline->layout(),
                /*descriptor_type=*/VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                /*image_infos=*/{
                    {0, {char_textures.char_image_map[character]
                             ->descriptor_info()}},
                });
            vertex_buffer->Draw(command_buffer, /*mesh_index=*/i,
                                /*instance_count=*/1);
          }
        });
  });
}

flat_hash_map<char, CharLoader::CharTextureInfo>
CharLoader::CreateCharTextureMap(
    const common::CharLib& char_lib, int font_height,
    CharTextures* char_textures) const {
  int total_width = 0;
  for (const auto& ch : char_lib.char_info_map()) {
    total_width += ch.second.advance;
  }
  char_textures->extent_after_merge = VkExtent2D{
      static_cast<uint32_t>(total_width),
      static_cast<uint32_t>(font_height),
  };
  const vec2 ratio = 1.0f / vec2{total_width, font_height};

  flat_hash_map<char, CharTextureInfo> char_resource_map;
  int offset_x = 0;
  for (const auto& ch : char_lib.char_info_map()) {
    const char character = ch.first;
    const auto& info = ch.second;
    char_resource_map[character] = CharTextureInfo{
        vec2{info.size} * ratio,
        vec2{info.bearing} * ratio,
        offset_x * ratio.x,
    };
    // space is a corner case where we won't have texture but will have advance
    if (character != ' ') {
      char_textures->char_image_map[character] =
          absl::make_unique<TextureImage>(
              context_,
              TextureBuffer::Info{
                  {info.data},
                  VK_FORMAT_R8_UNORM,
                  static_cast<uint32_t>(info.size.x),
                  static_cast<uint32_t>(info.size.y),
                  /*channel=*/1,
              }
          );
    }
    offset_x += info.advance;
  }
  return char_resource_map;
}

std::unique_ptr<RenderPass> CharLoader::CreateRenderPass(
    const VkExtent2D& target_extent,
    RenderPassBuilder::GetImage&& get_target_image) const {
  return RenderPassBuilder{context_}
      .set_framebuffer_size(target_extent)
      .set_num_framebuffer(1)
      .set_attachment(
          /*index=*/0,
          RenderPassBuilder::Attachment{
              /*attachment_ops=*/RenderPassBuilder::Attachment::ColorOps{
                  /*load_color=*/VK_ATTACHMENT_LOAD_OP_CLEAR,
                  /*store_color=*/VK_ATTACHMENT_STORE_OP_STORE,
              },
              /*initial_layout=*/VK_IMAGE_LAYOUT_UNDEFINED,
              /*final_layout=*/VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          },
          std::move(get_target_image)
      )
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
      })
      .Build();
}

std::unique_ptr<DynamicDescriptor> CharLoader::CreateDescriptor() const {
  return absl::make_unique<DynamicDescriptor>(
      context_,
      /*infos=*/vector<Descriptor::Info>{
          Descriptor::Info{
              /*descriptor_type=*/VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
              /*bindings=*/{
                  Descriptor::Info::Binding{
                      Descriptor::ResourceType::kTextureDiffuse,
                      /*binding_point=*/0,
                      /*array_length=*/1,
                  },
              },
          },
      });
}

std::unique_ptr<Pipeline> CharLoader::CreatePipeline(
    const VkExtent2D& target_extent, const RenderPass& render_pass,
    const DynamicDescriptor& descriptor) const {
  return PipelineBuilder{context_}
      .set_vertex_input(
          GetBindingDescriptions({GetPerVertexBindings<VertexAttrib2D>()}),
          GetAttributeDescriptions({GetVertexAttributes<VertexAttrib2D>()}))
      .set_layout({descriptor.layout()}, /*push_constant_ranges=*/{})
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
      .disable_depth_test()
      .Build();
}

std::unique_ptr<PerVertexBuffer> CharLoader::CreateVertexBuffer(
    vector<char>* char_merge_order) const {
  char_merge_order->reserve(char_texture_map_.size());

  const uint32_t indices[]{0, 1, 2, 0, 2, 3};
  vector<VertexAttrib2D> vertices;
  vertices.reserve(4 * char_texture_map_.size());
  for (const auto& ch : char_texture_map_) {
    char_merge_order->emplace_back(ch.first);
    const CharTextureInfo& char_info = ch.second;
    const vec2 offset = vec2{char_info.offset_x, 0.0f} + char_info.bearing;
    const vec2 size = char_info.size;
    vertices.emplace_back(VertexAttrib2D{
        /*pos=*/offset + vec2{0.0f, size.y},
        /*tex_coord=*/{0.0f, 0.0f},
    });
    vertices.emplace_back(VertexAttrib2D{
        /*pos=*/offset + vec2{size.x, size.y},
        /*tex_coord=*/{1.0f, 0.0f},
    });
    vertices.emplace_back(VertexAttrib2D{
        /*pos=*/offset + vec2{size.x, 0.0f},
        /*tex_coord=*/{1.0f, 1.0f},
    });
    vertices.emplace_back(VertexAttrib2D{
        /*pos=*/offset + vec2{0.0f, 0.0f},
        /*tex_coord=*/{0.0f, 1.0f},
    });
  }

  std::unique_ptr<PerVertexBuffer> buffer =
      absl::make_unique<PerVertexBuffer>(context_);
  buffer->Init(PerVertexBuffer::InfoReuse{
      /*num_mesh=*/static_cast<int>(char_texture_map_.size()),
      /*per_mesh_vertices=*/PerVertexBuffer::DataInfo{
          vertices.data(),
          sizeof(vertices[0]),
          /*unit_count=*/4,
      },
      /*shared_indices=*/PerVertexBuffer::DataInfo{
          indices,
          sizeof(indices[0]),
          sizeof(indices) / sizeof(indices[0]),
      },
  });
  return buffer;
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
