//
//  path_dumper.cc
//
//  Created by Pujun Lun on 3/28/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/viewer/path_dumper.h"

#include <algorithm>

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/common/timer.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "jessie_steamer/wrapper/vulkan/render_pass_util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

// To save device memory, we reuse images in this way:
//   - Render paths: [output] distance_field_image
//   - Bold paths: [input]  distance_field_image
//                 [output] paths_image
//   - Generate distance field: [input] paths_image
//                              [output] distance_field_image
// Note that 'paths_image_' has one channel, while 'distance_field_image_' has
// four channels.
enum ProcessingStage {
  kRenderPathsStage,
  kBoldPathsStage,
  kGenerateDistanceFieldStage,
  kNumProcessingStages,
};

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
  ASSERT_TRUE(common::util::IsPowerOf2(paths_image_dimension),
              absl::StrFormat("'paths_image_dimension' is expected to be power "
                              "of 2, while %d provided",
                              paths_image_dimension));

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

  auto paths_image_usage = image::UsageInfo{"Aurora paths"}
      .AddUsage(kBoldPathsStage, image::Usage::kLinearWriteInComputeShader)
      .AddUsage(kGenerateDistanceFieldStage,
                image::Usage::kLinearReadInComputeShader)
      .SetFinalUsage(image::Usage::kSampledInFragmentShader);
  paths_image_ = absl::make_unique<OffscreenImage>(
      context_, paths_image_extent, common::kBwImageChannel,
      image::GetImageUsageFlags(paths_image_usage), sampler_config);

  auto distance_field_image_usage = image::UsageInfo{"Distance field"}
      .SetInitialUsage(image::Usage::kSampledInFragmentShader)
      .AddUsage(kBoldPathsStage, image::Usage::kLinearReadInComputeShader)
      .AddUsage(kGenerateDistanceFieldStage,
                image::Usage::kLinearReadWriteInComputeShader)
      .SetFinalUsage(image::Usage::kSampledInFragmentShader);
  distance_field_image_ = absl::make_unique<OffscreenImage>(
      context_, paths_image_extent, common::kRgbaImageChannel,
      image::GetImageUsageFlags(distance_field_image_usage), sampler_config);

  image_layout_manager_ = absl::make_unique<image::LayoutManager>(
      kNumProcessingStages, image::LayoutManager::UsageInfoMap{
          {&paths_image_->image(), std::move(paths_image_usage)},
          {&distance_field_image_->image(),
           std::move(distance_field_image_usage)},
      });

  /* Graphics and compute */
  path_renderer_ = absl::make_unique<PathRenderer2D>(
      context_, /*intermediate_image=*/*distance_field_image_,
      /*output_image=*/*paths_image_, MultisampleImage::Mode::kBestEffect,
      std::move(aurora_paths_vertex_buffers));

  distance_field_generator_ = absl::make_unique<DistanceFieldGenerator>(
      context_, /*input_image=*/*paths_image_,
      /*output_image=*/*distance_field_image_);
}

void PathDumper::DumpAuroraPaths(const glm::vec3& viewpoint_position) {
  camera_->UpdateDirection(viewpoint_position);

#ifndef NDEBUG
  common::BasicTimer timer;
#endif /* !NDEBUG */

  const OneTimeCommand command{context_, &context_->queues().graphics_queue()};
  command.Run([this](const VkCommandBuffer& command_buffer) {
    /* Render and bold paths */
    image_layout_manager_->InsertMemoryBarrierBeforeStage(
        command_buffer, context_->queues().graphics_queue().family_index,
        kRenderPathsStage);
    path_renderer_->RenderPaths(command_buffer, *camera_);

    image_layout_manager_->InsertMemoryBarrierBeforeStage(
        command_buffer, context_->queues().graphics_queue().family_index,
        kBoldPathsStage);
    path_renderer_->BoldPaths(command_buffer);

    /* Generate distance field */
    image_layout_manager_->InsertMemoryBarrierBeforeStage(
        command_buffer, context_->queues().compute_queue().family_index,
        kGenerateDistanceFieldStage);
    distance_field_generator_->Generate(command_buffer);

    image_layout_manager_->InsertMemoryBarrierAfterFinalStage(
        command_buffer, context_->queues().graphics_queue().family_index);
  });

#ifndef NDEBUG
  LOG_INFO << absl::StreamFormat(
      "Elapsed time for generating distance map: %fs",
      timer.GetElapsedTimeSinceLaunch());
#endif /* !NDEBUG */
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
