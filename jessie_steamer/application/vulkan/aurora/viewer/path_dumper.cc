//
//  path_dumper.cc
//
//  Created by Pujun Lun on 3/28/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/viewer/path_dumper.h"

#include <algorithm>

#include "jessie_steamer/common/timer.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
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
//   - Bold paths: [input] distance_field_image
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

} /* namespace */

PathDumper::PathDumper(
    SharedBasicContext context, int paths_image_dimension,
    std::vector<const PerVertexBuffer*>&& aurora_paths_vertex_buffers)
    : context_{std::move(FATAL_IF_NULL(context))} {
  ASSERT_TRUE(common::util::IsPowerOf2(paths_image_dimension),
              absl::StrFormat("'paths_image_dimension' is expected to be power "
                              "of 2, while %d provided",
                              paths_image_dimension));

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
      paths_image_usage.GetAllUsages(), sampler_config);

  auto distance_field_image_usage = image::UsageInfo{"Distance field"}
      .SetInitialUsage(image::Usage::kSampledInFragmentShader)
      .AddUsage(kBoldPathsStage, image::Usage::kLinearReadInComputeShader)
      .AddUsage(kGenerateDistanceFieldStage,
                image::Usage::kLinearReadWriteInComputeShader)
      .SetFinalUsage(image::Usage::kSampledInFragmentShader);
  distance_field_image_ = absl::make_unique<OffscreenImage>(
      context_, paths_image_extent, common::kRgbaImageChannel,
      distance_field_image_usage.GetAllUsages(), sampler_config);

  image_layout_manager_ = absl::make_unique<image::LayoutManager>(
      kNumProcessingStages, image::LayoutManager::UsageInfoMap{
          {&paths_image_->image(), std::move(paths_image_usage)},
          {&distance_field_image_->image(),
           std::move(distance_field_image_usage)},
      });

  /* Graphics and compute pipelines */
  path_renderer_ = absl::make_unique<PathRenderer2D>(
      context_, /*intermediate_image=*/*distance_field_image_,
      /*output_image=*/*paths_image_, MultisampleImage::Mode::kBestEffect,
      std::move(aurora_paths_vertex_buffers));

  distance_field_generator_ = absl::make_unique<DistanceFieldGenerator>(
      context_, /*input_image=*/*paths_image_,
      /*output_image=*/*distance_field_image_);
}

void PathDumper::DumpAuroraPaths(const common::Camera& camera) {
#ifndef NDEBUG
  common::BasicTimer timer;
#endif /* !NDEBUG */

  // TODO: Compute queue and graphics queue might be different queues.
  const OneTimeCommand command{context_, &context_->queues().graphics_queue()};
  command.Run([this, &camera](const VkCommandBuffer& command_buffer) {
    // Render and bold paths.
    image_layout_manager_->InsertMemoryBarrierBeforeStage(
        command_buffer, context_->queues().graphics_queue().family_index,
        kRenderPathsStage);
    path_renderer_->RenderPaths(command_buffer, camera);

    image_layout_manager_->InsertMemoryBarrierBeforeStage(
        command_buffer, context_->queues().graphics_queue().family_index,
        kBoldPathsStage);
    path_renderer_->BoldPaths(command_buffer);

    // Generate distance field.
    image_layout_manager_->InsertMemoryBarrierBeforeStage(
        command_buffer, context_->queues().compute_queue().family_index,
        kGenerateDistanceFieldStage);
    distance_field_generator_->Generate(command_buffer);

    image_layout_manager_->InsertMemoryBarrierAfterFinalStage(
        command_buffer, context_->queues().graphics_queue().family_index);
  });

#ifndef NDEBUG
  LOG_INFO << absl::StreamFormat("Elapsed time for dumping aurora paths: %fs",
                                 timer.GetElapsedTimeSinceLaunch());
#endif /* !NDEBUG */
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
