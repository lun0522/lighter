//
//  path_dumper.cc
//
//  Created by Pujun Lun on 3/28/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/vulkan/aurora/viewer/path_dumper.h"

#include <algorithm>
#include <array>

#include "lighter/common/timer.h"
#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/wrapper/command.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace renderer;
using namespace renderer::vulkan;

// To save device memory, we reuse images in this way:
//   - Render paths: [output] distance_field_image
//   - Bold paths: [input] distance_field_image
//                 [output] paths_image
//   - Generate distance field: [input] paths_image
//                              [output] distance_field_image
// Note that 'paths_image_' has one channel, while 'distance_field_image_' has
// four channels.
enum SubpassIndex {
  kBoldPathsSubpassIndex = 0,
  kGenerateDistanceFieldSubpassIndex,
  kNumSubpasses,
};

} /* namespace */

const std::string PathDumper::paths_image_name_ = "Aurora paths";
const std::string PathDumper::distance_field_image_name_ = "Distance field";

PathDumper::PathDumper(
    SharedBasicContext context, int paths_image_dimension,
    std::vector<const PerVertexBuffer*>&& aurora_paths_vertex_buffers)
    : context_{std::move(FATAL_IF_NULL(context))} {
  ASSERT_TRUE(common::util::IsPowerOf2(paths_image_dimension),
              absl::StrFormat("'paths_image_dimension' is expected to be power "
                              "of 2, while %d provided",
                              paths_image_dimension));

  /* Image */
  const VkExtent2D paths_image_extent{
      static_cast<uint32_t>(paths_image_dimension),
      static_cast<uint32_t>(paths_image_dimension)};
  const ImageSampler::Config sampler_config{
      VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};

  // TODO: We still need a high level description, especially when image layouts
  // change across files.
  ImageUsageHistory paths_image_usage_history{};
  paths_image_usage_history
      .AddUsage(kBoldPathsSubpassIndex,
                ImageUsage::GetLinearAccessInComputeShaderUsage(
                    ImageUsage::AccessType::kWriteOnly))
      .AddUsage(kGenerateDistanceFieldSubpassIndex,
                ImageUsage::GetLinearAccessInComputeShaderUsage(
                    ImageUsage::AccessType::kReadOnly))
      .SetFinalUsage(ImageUsage::GetSampledInFragmentShaderUsage());
  paths_image_ = absl::make_unique<OffscreenImage>(
      context_, paths_image_extent, common::kBwImageChannel,
      paths_image_usage_history.GetAllUsages(), sampler_config);

  ImageUsageHistory distance_field_image_usage_history{
      /*initial_usage=*/ImageUsage::GetMultisampleResolveTargetUsage()};
  distance_field_image_usage_history
      .AddUsage(kBoldPathsSubpassIndex,
                ImageUsage::GetLinearAccessInComputeShaderUsage(
                    ImageUsage::AccessType::kReadOnly))
      .AddUsage(kGenerateDistanceFieldSubpassIndex,
                ImageUsage::GetLinearAccessInComputeShaderUsage(
                    ImageUsage::AccessType::kReadWrite)
                    .set_use_high_precision())
      .SetFinalUsage(ImageUsage::GetSampledInFragmentShaderUsage());
  distance_field_image_ = absl::make_unique<OffscreenImage>(
      context_, paths_image_extent, common::kRgbaImageChannel,
      distance_field_image_usage_history.GetAllUsages(), sampler_config);

  /* Graphics and compute pipelines */
  compute_pass_ = absl::make_unique<ComputePass>(kNumSubpasses);
  (*compute_pass_)
      .AddImage(paths_image_name_, std::move(paths_image_usage_history))
      .AddImage(distance_field_image_name_,
                std::move(distance_field_image_usage_history));

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
  const common::BasicTimer timer;
#endif /* !NDEBUG */

  // TODO: Compute queue and graphics queue might be different queues.
  const OneTimeCommand command{context_, &context_->queues().graphics_queue()};
  command.Run([this, &camera](const VkCommandBuffer& command_buffer) {
    const std::array<ComputePass::ComputeOp, kNumSubpasses> compute_ops{
        [this, &command_buffer]() {
          path_renderer_->BoldPaths(command_buffer);
        },
        [this, &command_buffer]() {
          distance_field_generator_->Generate(command_buffer);
        },
    };
    path_renderer_->RenderPaths(command_buffer, camera);
    compute_pass_->Run(
        command_buffer, context_->queues().compute_queue().family_index,
        /*image_map=*/{
            {paths_image_name_, paths_image_.get()},
            {distance_field_image_name_, distance_field_image_.get()},
        },
        compute_ops);
  });

#ifndef NDEBUG
  LOG_INFO << absl::StreamFormat("Elapsed time for dumping aurora paths: %fs",
                                 timer.GetElapsedTimeSinceLaunch());
#endif /* !NDEBUG */
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */
