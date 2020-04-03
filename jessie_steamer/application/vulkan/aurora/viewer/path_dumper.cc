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
  kNumSubpasses,
  kNumOverlaySubpasses = kNumSubpasses - kDumpPathsSubpassIndex,
};

constexpr uint32_t kVertexBufferBindingPoint = 0;

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct Transformation {
  ALIGN_MAT4 glm::mat4 proj_view;
};

/* END: Consistent with uniform blocks defined in shaders. */

// Returns the axis of earth model in object space.
const glm::vec3& GetEarthModelAxis() {
  static const glm::vec3* earth_model_axis = nullptr;
  if (earth_model_axis == nullptr) {
    earth_model_axis = new glm::vec3{0.0f, 1.0f, 0.0f};
  }
  return *earth_model_axis;
}

} /* namespace */

PathDumper::PathDumper(
    const SharedBasicContext& context,
    int paths_image_dimension, float camera_field_of_view,
    std::vector<const PerVertexBuffer*>&& aurora_paths_vertex_buffers)
    : aurora_paths_vertex_buffers_{std::move(aurora_paths_vertex_buffers)} {
  /* Camera */
  common::Camera::Config config;
  // We assume that:
  // (1) Aurora paths points are on a unit sphere.
  // (2) The camera is located at the center of sphere.
  // The 'look_at' point doesn't matter at this moment. It will be updated to
  // user viewpoint.
  config.far = 2.0f;
  config.up = GetEarthModelAxis();
  config.position = glm::vec3{0.0f};
  config.look_at = glm::vec3{1.0f};
  const common::PerspectiveCamera::PersConfig pers_config{
      camera_field_of_view, /*fov_aspect_ratio=*/1.0f};
  camera_ = absl::make_unique<common::PerspectiveCamera>(config, pers_config);

  /* Command */
  command_ = absl::make_unique<OneTimeCommand>(
      context, &context->queues().graphics_queue());

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
      context, sizeof(Transformation), /*num_frames_in_flight=*/1);

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
  render_ops_.emplace_back([this](const VkCommandBuffer& command_buffer) {
    for (const auto* buffer : aurora_paths_vertex_buffers_) {
      buffer->Draw(command_buffer, kVertexBufferBindingPoint,
                   /*mesh_index=*/0, /*instance_count=*/1);
    }
  });

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
  camera_->UpdateDirection(viewpoint_position);
  trans_constant_->HostData<Transformation>(/*frame=*/0)->proj_view =
      camera_->projection() * camera_->view();
  command_->Run([this](const VkCommandBuffer& command_buffer) {
    pipeline_->Bind(command_buffer);
    trans_constant_->Flush(command_buffer, pipeline_->layout(), /*frame=*/0,
                           /*target_offset=*/0, VK_SHADER_STAGE_VERTEX_BIT);
    render_pass_->Run(command_buffer, /*framebuffer_index=*/0, render_ops_);
  });
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
