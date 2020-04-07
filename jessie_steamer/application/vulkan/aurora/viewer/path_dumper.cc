//
//  path_dumper.cc
//
//  Created by Pujun Lun on 3/28/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/viewer/path_dumper.h"

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/align.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "jessie_steamer/wrapper/vulkan/render_pass_util.h"
#include "third_party/absl/memory/memory.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

enum SubpassIndex {
  kPathsOperationSubpassIndex = 0,
  kNumSubpasses,
  kNumOverlaySubpasses = kNumSubpasses - kPathsOperationSubpassIndex,
};

constexpr uint32_t kVertexBufferBindingPoint = 0;
constexpr uint32_t kImageBindingPoint = 0;

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
    SharedBasicContext context,
    int paths_image_dimension, float camera_field_of_view,
    std::vector<const PerVertexBuffer*>&& aurora_paths_vertex_buffers)
    : context_{std::move(FATAL_IF_NULL(context))} {
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

  /* Render pass */
  const VkExtent2D paths_image_extent{
      static_cast<uint32_t>(paths_image_dimension),
      static_cast<uint32_t>(paths_image_dimension)};
  dump_paths_pass_ = absl::make_unique<DumpPathsPass>(
      context_, paths_image_extent, std::move(aurora_paths_vertex_buffers));
  bold_paths_pass_ = absl::make_unique<BoldPathsPass>(
      context_, dump_paths_pass_->paths_image(), paths_image_extent);
}

void PathDumper::DumpAuroraPaths(const glm::vec3& viewpoint_position) {
  camera_->UpdateDirection(viewpoint_position);
  dump_paths_pass_->UpdateData(*camera_);
  const OneTimeCommand command{context_, &context_->queues().graphics_queue()};
  command.Run([this](const VkCommandBuffer& command_buffer) {
    dump_paths_pass_->Draw(command_buffer);
    bold_paths_pass_->Draw(command_buffer);
  });
}

PathDumper::DumpPathsPass::DumpPathsPass(
    const SharedBasicContext& context, const VkExtent2D& image_extent,
    std::vector<const PerVertexBuffer*>&& aurora_paths_vertex_buffers)
    : aurora_paths_vertex_buffers_{std::move(aurora_paths_vertex_buffers)} {
  /* Image */
  const SamplableImage::Config image_config{
      VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
  paths_image_ = absl::make_unique<OffscreenImage>(
      context, OffscreenImage::DataSource::kRender, common::kBwImageChannel,
      image_extent, image_config);
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
      NaiveRenderPassBuilder::ColorAttachmentFinalUsage::kSampledAsTexture,
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
  pipeline_ = GraphicsPipelineBuilder{context}
      .SetPipelineName("Dump aurora path")
      .SetMultisampling(multisample_image_->sample_count())
      .SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
      .AddVertexInput(
          kVertexBufferBindingPoint,
          pipeline::GetPerVertexBindingDescription<common::Vertex3DPosOnly>(),
          aurora_paths_vertex_buffers_[0]->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout(/*descriptor_layouts=*/{},
                         {trans_constant_->MakePerFrameRange(
                             VK_SHADER_STAGE_VERTEX_BIT)})
      .SetViewport(pipeline::GetFullFrameViewport(image_extent))
      .SetRenderPass(**render_pass_, kPathsOperationSubpassIndex)
      .SetColorBlend({pipeline::GetColorBlendState(/*enable_blend=*/false)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("aurora/dump_path.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("aurora/dump_path.frag"))
      .Build();
}

void PathDumper::DumpPathsPass::UpdateData(const common::Camera& camera) {
  trans_constant_->HostData<Transformation>(/*frame=*/0)->proj_view =
      camera.projection() * camera.view();
}

void PathDumper::DumpPathsPass::Draw(const VkCommandBuffer& command_buffer) {
  pipeline_->Bind(command_buffer);
  trans_constant_->Flush(command_buffer, pipeline_->layout(), /*frame=*/0,
                         /*target_offset=*/0, VK_SHADER_STAGE_VERTEX_BIT);
  render_pass_->Run(command_buffer, /*framebuffer_index=*/0, render_ops_);
}

PathDumper::BoldPathsPass::BoldPathsPass(const SharedBasicContext& context,
                                         const SamplableImage& paths_image,
                                         const VkExtent2D& image_extent) {
  using common::Vertex2D;

  /* Descriptor */
  descriptor_ = absl::make_unique<StaticDescriptor>(
      context, /*infos=*/std::vector<Descriptor::Info>{
          Descriptor::Info{
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              VK_SHADER_STAGE_FRAGMENT_BIT,
              /*bindings=*/{
                  Descriptor::Info::Binding{
                      kImageBindingPoint,
                      /*array_length=*/1,
                  }},
          }});
  descriptor_->UpdateImageInfos(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      /*image_info_map=*/
      {{kImageBindingPoint, {paths_image.GetDescriptorInfo()}}});

  /* Vertex buffer */
  const auto vertex_data =
      Vertex2D::GetFullScreenSquadVertices(/*flip_y=*/false);
  const PerVertexBuffer::NoIndicesDataInfo vertex_data_info{
      /*per_mesh_vertices=*/{{PerVertexBuffer::VertexDataInfo{vertex_data}}}
  };
  vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
      context, vertex_data_info, pipeline::GetVertexAttribute<Vertex2D>());

  /* Image */
  const SamplableImage::Config image_config{
      VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
  bold_paths_image_ = absl::make_unique<OffscreenImage>(
      context, OffscreenImage::DataSource::kRender, common::kBwImageChannel,
      image_extent, image_config);

  /* Render pass */
  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/false,
      /*num_transparent_subpasses=*/0,
      kNumOverlaySubpasses,
  };
  // TODO: Change usage to kAccessedByHost.
  NaiveRenderPassBuilder render_pass_builder{
      context, subpass_config, /*num_framebuffers=*/1,
      /*use_multisampling=*/false,
      NaiveRenderPassBuilder::ColorAttachmentFinalUsage::kSampledAsTexture,
  };
  (*render_pass_builder.mutable_builder())
      .UpdateAttachmentImage(
          render_pass_builder.color_attachment_index(),
          [this](int) -> const Image& { return *bold_paths_image_; });
  render_pass_ = render_pass_builder->Build();
  render_ops_.emplace_back([this](const VkCommandBuffer& command_buffer) {
    vertex_buffer_->Draw(command_buffer, kVertexBufferBindingPoint,
                         /*mesh_index=*/0, /*instance_count=*/1);
  });

  /* Pipeline */
  pipeline_ = GraphicsPipelineBuilder{context}
      .SetPipelineName("Bold aurora path")
      .AddVertexInput(kVertexBufferBindingPoint,
                      pipeline::GetPerVertexBindingDescription<Vertex2D>(),
                      vertex_buffer_->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout({descriptor_->layout()}, /*push_constant_ranges=*/{})
      .SetViewport(pipeline::GetFullFrameViewport(image_extent))
      .SetRenderPass(**render_pass_, kPathsOperationSubpassIndex)
      .SetColorBlend({pipeline::GetColorBlendState(/*enable_blend=*/false)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("aurora/bold_path.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("aurora/bold_path.frag"))
      .Build();
}

void PathDumper::BoldPathsPass::Draw(const VkCommandBuffer& command_buffer) {
  pipeline_->Bind(command_buffer);
  descriptor_->Bind(command_buffer, pipeline_->layout(),
                    pipeline_->binding_point());
  render_pass_->Run(command_buffer, /*framebuffer_index=*/0, render_ops_);
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
