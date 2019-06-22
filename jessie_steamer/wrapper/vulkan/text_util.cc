//
//  text_util.cc
//
//  Created by Pujun Lun on 6/20/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/text_util.h"

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/vertex_input_util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::string;
using std::vector;

using common::VertexAttrib2D;

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

  int total_width;
  auto char_resource_map = LoadCharResource(char_lib, &total_width);
  VkExtent2D image_extent{static_cast<uint32_t>(total_width),
                          static_cast<uint32_t>(font_height)};
  OffscreenImage image{context, /*channel=*/1, image_extent};
  auto render_pass = CreateRenderPass(
      image_extent, [&image](int index) -> const Image& { return image; });
  auto descriptors = CreateDescriptors();  // TODO
  auto pipeline = CreatePipeline(image_extent, *render_pass, descriptors);
}

absl::flat_hash_map<char, CharLoader::CharResourceInfo>
CharLoader::LoadCharResource(const common::CharLib& char_lib,
                             int* total_width) const {
  absl::flat_hash_map<char, CharResourceInfo> char_resource_map;
  int offset_x = 0;
  for (const auto& ch : char_lib.char_info_map()) {
    const auto& info = ch.second;
    char_resource_map[ch.first] = CharResourceInfo{
        info.size,
        info.bearing,
        offset_x,
        absl::make_unique<TextureImage>(
            context_,
            TextureBuffer::Info{
                {info.data},
                VK_FORMAT_R8_UNORM,
                static_cast<uint32_t>(info.size.x),
                static_cast<uint32_t>(info.size.y),
                /*channel=*/1,
            }
        ),
    };
    offset_x += info.advance;
  }
  *total_width = offset_x;
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

std::vector<StaticDescriptor> CharLoader::CreateDescriptors() const {
  return {};
}

std::unique_ptr<Pipeline> CharLoader::CreatePipeline(
    const VkExtent2D& target_extent, const RenderPass& render_pass,
    const vector<StaticDescriptor>& descriptors) const {
  vector<VkDescriptorSetLayout> descriptor_layouts;
  descriptor_layouts.reserve(descriptors.size());
  for (const auto& descriptor : descriptors) {
    descriptor_layouts.emplace_back(descriptor.layout());
  }

  return PipelineBuilder{context_}
      .set_vertex_input(
          GetBindingDescriptions({GetPerVertexBindings<VertexAttrib2D>()}),
          GetAttributeDescriptions({GetVertexAttributes<VertexAttrib2D>()}))
      .set_layout(std::move(descriptor_layouts), /*push_constant_ranges=*/{})
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

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
