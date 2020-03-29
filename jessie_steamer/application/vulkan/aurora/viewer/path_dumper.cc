//
//  path_dumper.cc
//
//  Created by Pujun Lun on 3/28/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/viewer/path_dumper.h"

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/align.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "jessie_steamer/wrapper/vulkan/render_pass_util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

enum SubpassIndex {
  kDumpPathsSubpassIndex = 0,
  kNumOverlaySubpasses,
};

constexpr uint32_t kVertexBufferBindingPoint = 0;

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct Transformation {
  ALIGN_MAT4 glm::mat4 projection;
};

/* END: Consistent with uniform blocks defined in shaders. */

} /* namespace */

PathDumper::PathDumper(
    const SharedBasicContext& context,
    int num_frames_in_flight, int paths_image_dimension,
    std::vector<const PerVertexBuffer*>&& aurora_paths_vertex_buffers)
    : aurora_paths_vertex_buffers_{std::move(aurora_paths_vertex_buffers)} {
  /* Image */
  const VkExtent2D paths_image_extent{
      static_cast<uint32_t>(paths_image_dimension),
      static_cast<uint32_t>(paths_image_dimension)};
  paths_image_ = absl::make_unique<OffscreenImage>(
      context, /*channel=*/1, paths_image_extent, SamplableImage::Config{});
  multisample_image_ = MultisampleImage::CreateColorMultisampleImage(
      context, *paths_image_, MultisampleImage::Mode::kBestEffect);

  /* Push constant */
  trans_constant_ = absl::make_unique<PushConstant>(
      context, sizeof(Transformation), num_frames_in_flight);

  /* Render pass */
  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/false,
      /*num_transparent_subpasses=*/0,
      kNumOverlaySubpasses,
  };
  NaiveRenderPassBuilder render_pass_builder{
      context, subpass_config, /*num_framebuffers=*/1,
      /*use_multisampling=*/true,
      NaiveRenderPassBuilder::ColorAttachmentFinalUsage::kAccessedByHost,
  };
  (*render_pass_builder.mutable_builder())
      .UpdateAttachmentImage(
          render_pass_builder.color_attachment_index(),
          [this](int) -> const Image& { return *paths_image_; })
      .UpdateAttachmentImage(
          render_pass_builder.multisample_attachment_index(),
          [this](int) -> const Image& { return *multisample_image_; });
  render_pass_ = render_pass_builder->Build();

  /* Pipeline */
  pipeline_ = PipelineBuilder{context}
      .SetName("Dump aurora path")
      .SetMultisampling(multisample_image_->sample_count())
      .SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
      .AddVertexInput(
          kVertexBufferBindingPoint,
          pipeline::GetPerVertexBindingDescription<common::Vertex3DPosOnly>(),
          aurora_paths_vertex_buffers_[0]->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout(/*descriptor_layouts=*/{},
                         {trans_constant_->MakePerFrameRange(
                             VK_SHADER_STAGE_VERTEX_BIT)})
      .SetViewport(pipeline::GetFullFrameViewport(paths_image_extent))
      .SetRenderPass(**render_pass_, kDumpPathsSubpassIndex)
      .SetColorBlend({pipeline::GetColorBlendState(/*enable_blend=*/false)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("dump_path.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("dump_path.frag"))
      .Build();
}

void PathDumper::DumpAuroraPaths(const glm::vec3& viewpoint_position) {

}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
