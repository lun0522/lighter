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
#include "third_party/absl/types/span.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

enum ProcessingStage {
  kRenderPathsStage,
  kGenerateDistanceFieldStage,
  kNumProcessingStages,
};

enum SubpassIndex {
  kDumpPathsSubpassIndex = 0,
  kNumSubpasses,
  kNumOverlaySubpasses = kNumSubpasses - kDumpPathsSubpassIndex,
};

constexpr uint32_t kVertexBufferBindingPoint = 0;

/* BEGIN: Consistent with work group size defined in shaders. */

constexpr uint32_t kWorkGroupSizeX = 32;
constexpr uint32_t kWorkGroupSizeY = 32;

/* END: Consistent with work group size defined in shaders. */

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

  /* Image and layout manager */
  const VkExtent2D paths_image_extent{
      static_cast<uint32_t>(paths_image_dimension),
      static_cast<uint32_t>(paths_image_dimension)};
  const ImageSampler::Config sampler_config{
      VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};

  auto ping_image_usage = image::UsageInfo{"Ping"}
      .SetInitialUsage(image::Usage::kSampledInFragmentShader)
      .AddUsage(kGenerateDistanceFieldStage,
                image::Usage::kLinearReadWriteInComputeShader)
      .SetFinalUsage(image::Usage::kSampledInFragmentShader);
  images_[kPingImageIndex] = absl::make_unique<OffscreenImage>(
      context_, paths_image_extent, common::kBwImageChannel,
      image::GetImageUsageFlags(ping_image_usage), sampler_config);

  auto pong_image_usage = image::UsageInfo{"Pong"}
      .AddUsage(kGenerateDistanceFieldStage,
                image::Usage::kLinearReadWriteInComputeShader)
      .SetFinalUsage(image::Usage::kSampledInFragmentShader);
  images_[kPongImageIndex] = absl::make_unique<OffscreenImage>(
      context_, paths_image_extent, common::kBwImageChannel,
      image::GetImageUsageFlags(pong_image_usage), sampler_config);

  image_layout_manager_ = absl::make_unique<image::LayoutManager>(
      kNumProcessingStages, image::LayoutManager::UsageInfoMap{
          {&images_[kPingImageIndex]->image(), std::move(ping_image_usage)},
          {&images_[kPongImageIndex]->image(), std::move(pong_image_usage)},
      });

  /* Render pass */
  path_renderer_ = absl::make_unique<PathRenderer>(
      context_, *images_[kPingImageIndex],
      std::move(aurora_paths_vertex_buffers));

  /* Computer shader */
  distance_field_generator_ = absl::make_unique<DistanceFieldGenerator>(
      context_, images_, *image_layout_manager_);
}

void PathDumper::DumpAuroraPaths(const glm::vec3& viewpoint_position) {
  camera_->UpdateDirection(viewpoint_position);
  path_renderer_->UpdateData(*camera_);

  const OneTimeCommand command{context_, &context_->queues().graphics_queue()};
  command.Run([this](const VkCommandBuffer& command_buffer) {
    image_layout_manager_->InsertMemoryBarrierBeforeStage(
        command_buffer, context_->queues().graphics_queue().family_index,
        kRenderPathsStage);
    path_renderer_->Draw(command_buffer);

    image_layout_manager_->InsertMemoryBarrierBeforeStage(
        command_buffer, context_->queues().compute_queue().family_index,
        kGenerateDistanceFieldStage);

    distance_field_generator_->Generate(command_buffer, images_,
                                        *image_layout_manager_);

    image_layout_manager_->InsertMemoryBarrierAfterFinalStage(
        command_buffer, context_->queues().compute_queue().family_index);
  });
}

PathDumper::PathRenderer::PathRenderer(
    const SharedBasicContext& context, const Image& paths_image,
    std::vector<const PerVertexBuffer*>&& aurora_paths_vertex_buffers)
    : aurora_paths_vertex_buffers_{std::move(aurora_paths_vertex_buffers)} {
  /* Image */
  multisample_image_ = MultisampleImage::CreateColorMultisampleImage(
      context, paths_image, MultisampleImage::Mode::kBestEffect);

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
          [&paths_image](int) -> const Image& { return paths_image; })
      .UpdateAttachmentImage(
          render_pass_builder.multisample_attachment_index(),
          [this](int) -> const Image& { return *multisample_image_; });
  render_pass_ = render_pass_builder->Build();
  render_op_ = [this](const VkCommandBuffer& command_buffer) {
    for (const auto* buffer : aurora_paths_vertex_buffers_) {
      buffer->Draw(command_buffer, kVertexBufferBindingPoint,
                   /*mesh_index=*/0, /*instance_count=*/1);
    }
  };

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
      .SetViewport(pipeline::GetFullFrameViewport(paths_image.extent()))
      .SetRenderPass(**render_pass_, kDumpPathsSubpassIndex)
      .SetColorBlend(
          {pipeline::GetColorAlphaBlendState(/*enable_blend=*/false)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("aurora/dump_path.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("aurora/dump_path.frag"))
      .Build();
}

void PathDumper::PathRenderer::UpdateData(const common::Camera& camera) {
  trans_constant_->HostData<Transformation>(/*frame=*/0)->proj_view =
      camera.projection() * camera.view();
}

void PathDumper::PathRenderer::Draw(const VkCommandBuffer& command_buffer) {
  pipeline_->Bind(command_buffer);
  trans_constant_->Flush(command_buffer, pipeline_->layout(), /*frame=*/0,
                         /*target_offset=*/0, VK_SHADER_STAGE_VERTEX_BIT);
  render_pass_->Run(command_buffer, /*framebuffer_index=*/0,
                    absl::MakeSpan(&render_op_, 1));
}

PathDumper::DistanceFieldGenerator::DistanceFieldGenerator(
    const SharedBasicContext& context,
    const std::array<std::unique_ptr<OffscreenImage>, kNumImages>& images,
    const image::LayoutManager& image_layout_manager) {
  enum UniformBindingPoint {
    kOriginalImageBindingPoint = 0,
    kOutputImageBindingPoint,
  };

  /* Descriptor */
  std::vector<Descriptor::Info> descriptor_infos;
  descriptor_infos.reserve(kNumImages);
  for (auto binding_point : {kOriginalImageBindingPoint,
                             kOutputImageBindingPoint}) {
    descriptor_infos.emplace_back(Descriptor::Info{
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        VK_SHADER_STAGE_COMPUTE_BIT,
        /*bindings=*/{{binding_point, /*array_length=*/1}},
    });
  }
  for (auto& descriptor : descriptors_) {
    descriptor = absl::make_unique<StaticDescriptor>(context, descriptor_infos);
  }

  // TODO
  auto ping_image_descriptor_info =
      images[kPingImageIndex]->GetDescriptorInfo();
  ping_image_descriptor_info.imageLayout =
      image_layout_manager.GetLayoutAtStage(
          images[kPingImageIndex]->image(), kGenerateDistanceFieldStage);
  auto pong_image_descriptor_info =
      images[kPongImageIndex]->GetDescriptorInfo();
  pong_image_descriptor_info.imageLayout =
      image_layout_manager.GetLayoutAtStage(
          images[kPongImageIndex]->image(), kGenerateDistanceFieldStage);
  descriptors_[kPingImageIndex]->UpdateImageInfos(
      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      /*image_info_map=*/{
          {kOriginalImageBindingPoint, {ping_image_descriptor_info}},
          {kOutputImageBindingPoint, {pong_image_descriptor_info}}});
  descriptors_[kPongImageIndex]->UpdateImageInfos(
      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      /*image_info_map=*/{
          {kOriginalImageBindingPoint, {pong_image_descriptor_info}},
          {kOutputImageBindingPoint, {ping_image_descriptor_info}}});

  /* Pipeline */
  bold_path_pipeline_ = ComputePipelineBuilder{context}
      .SetPipelineName("Bold path")
      .SetPipelineLayout({descriptors_[kPingImageIndex]->layout()},
                         /*push_constant_ranges=*/{})
      .SetShader(common::file::GetVkShaderPath("aurora/bold_path.comp"))
      .Build();

  distance_field_pipeline_ = ComputePipelineBuilder{context}
      .SetPipelineName("Generate distance field")
      .SetPipelineLayout({descriptors_[kPingImageIndex]->layout()},
                         /*push_constant_ranges=*/{})
      .SetShader(common::file::GetVkShaderPath("aurora/distance_field.comp"))
      .Build();
}

void PathDumper::DistanceFieldGenerator::Generate(
    const VkCommandBuffer& command_buffer,
    const std::array<std::unique_ptr<OffscreenImage>, kNumImages>& images,
    const image::LayoutManager& image_layout_manager) {
  const auto& image_extent = images[kPingImageIndex]->extent();
  const auto group_count_x = util::GetWorkGroupCount(image_extent.width,
                                                     kWorkGroupSizeX);
  const auto group_count_y = util::GetWorkGroupCount(image_extent.height,
                                                     kWorkGroupSizeY);

  bold_path_pipeline_->Bind(command_buffer);
  descriptors_[kPingImageIndex]->Bind(
      command_buffer, bold_path_pipeline_->layout(),
      bold_path_pipeline_->binding_point());
  vkCmdDispatch(command_buffer, group_count_x, group_count_y,
                /*groupCountZ=*/1);

  distance_field_pipeline_->Bind(command_buffer);
  descriptors_[kPongImageIndex]->Bind(
      command_buffer, distance_field_pipeline_->layout(),
      distance_field_pipeline_->binding_point());
  vkCmdDispatch(command_buffer, group_count_x, group_count_y,
                /*groupCountZ=*/1);
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
