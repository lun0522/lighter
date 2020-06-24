//
//  distance_field.cc
//
//  Created by Pujun Lun on 4/18/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/vulkan/aurora/viewer/distance_field.h"

#include <algorithm>

#include "lighter/common/util.h"
#include "lighter/wrapper/vulkan/align.h"
#include "lighter/wrapper/vulkan/image_util.h"
#include "lighter/wrapper/vulkan/util.h"
#include "third_party/absl/memory/memory.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

enum ImageBindingPoint {
  kOriginalImageBindingPoint = 0,
  kOutputImageBindingPoint,
};

enum ProcessingStage {
  kGenerateDistanceFieldStage,
  kNumProcessingStages,
};

/* BEGIN: Consistent with work group size defined in shaders. */

constexpr uint32_t kWorkGroupSizeX = 16;
constexpr uint32_t kWorkGroupSizeY = 16;

/* END: Consistent with work group size defined in shaders. */

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct StepWidth {
  ALIGN_SCALAR(int) int value;
};

/* END: Consistent with uniform blocks defined in shaders. */

} /* namespace */

DistanceFieldGenerator::DistanceFieldGenerator(
    const SharedBasicContext& context,
    const OffscreenImage& input_image, const OffscreenImage& output_image)
    : work_group_count_{util::GetWorkGroupCount(
          input_image.extent(), {kWorkGroupSizeX, kWorkGroupSizeY})} {
  const auto& image_extent = input_image.extent();
  ASSERT_TRUE(output_image.extent().width == image_extent.width &&
                  output_image.extent().height == image_extent.height,
              "Size of input and output images must match");

  /* Push constant */
  const int greatest_dimension = std::max(image_extent.width,
                                          image_extent.height);
  std::vector<int> step_widths;
  int dimension = 1;
  while (dimension < greatest_dimension) {
    step_widths.push_back(dimension);
    dimension *= 2;
  }
  num_steps_ = step_widths.size();

  step_width_constant_ = absl::make_unique<PushConstant>(
      context, sizeof(StepWidth), num_steps_);
  for (int i = 0; i < num_steps_; ++i) {
    step_width_constant_->HostData<StepWidth>(/*frame=*/i)->value =
        step_widths[i];
  }
  const auto push_constant_range =
      step_width_constant_->MakePerFrameRange(VK_SHADER_STAGE_COMPUTE_BIT);

  /* Image */
  auto pong_image_usage = image::UsageInfo{"Pong"}
      .AddUsage(kGenerateDistanceFieldStage,
                image::Usage::GetLinearAccessInComputeShaderUsage(
                    image::Usage::AccessType::kReadWrite)
                    .set_use_high_precision());
  pong_image_ = absl::make_unique<OffscreenImage>(
      context, image_extent, output_image.format(),
      pong_image_usage.GetAllUsages(), ImageSampler::Config{});
  const image::LayoutManager layout_manager{
      kNumProcessingStages, /*usage_info_map=*/{
          {&pong_image_->image(), std::move(pong_image_usage)},
      }};

  /* Descriptor */
  descriptor_ = absl::make_unique<DynamicDescriptor>(
      context, std::vector<Descriptor::Info>{
          Descriptor::Info{
              Image::GetDescriptorTypeForLinearAccess(),
              VK_SHADER_STAGE_COMPUTE_BIT,
              /*bindings=*/{
                  {kOriginalImageBindingPoint, /*array_length=*/1},
                  {kOutputImageBindingPoint, /*array_length=*/1},
              },
          },
      });

  const auto image_layout = layout_manager.GetLayoutAtStage(
      pong_image_->image(), kGenerateDistanceFieldStage);
  const auto input_image_descriptor_info =
      input_image.GetDescriptorInfo(image_layout);
  const auto ping_image_descriptor_info =
      output_image.GetDescriptorInfo(image_layout);
  const auto pong_image_descriptor_info =
      pong_image_->GetDescriptorInfo(image_layout);

  image_info_maps_[kInputToPing] = {
      {kOriginalImageBindingPoint, {input_image_descriptor_info}},
      {kOutputImageBindingPoint, {ping_image_descriptor_info}}};
  image_info_maps_[kPingToPong] = {
      {kOriginalImageBindingPoint, {ping_image_descriptor_info}},
      {kOutputImageBindingPoint, {pong_image_descriptor_info}}};
  image_info_maps_[kPongToPing] = {
      {kOriginalImageBindingPoint, {pong_image_descriptor_info}},
      {kOutputImageBindingPoint, {ping_image_descriptor_info}}};
  image_info_maps_[kPingToPing] = {
      {kOriginalImageBindingPoint, {ping_image_descriptor_info}},
      {kOutputImageBindingPoint, {ping_image_descriptor_info}}};

  /* Pipeline */
  path_to_coord_pipeline_ = ComputePipelineBuilder{context}
      .SetPipelineName("Path to coordinate")
      .SetPipelineLayout({descriptor_->layout()}, /*push_constant_ranges=*/{})
      .SetShader(common::file::GetVkShaderPath("aurora/path_to_coord.comp"))
      .Build();

  jump_flooding_pipeline_ = ComputePipelineBuilder{context}
      .SetPipelineName("Jump flooding")
      .SetPipelineLayout({descriptor_->layout()}, {push_constant_range})
      .SetShader(common::file::GetVkShaderPath("aurora/jump_flooding.comp"))
      .Build();

  coord_to_dist_pipeline_ = ComputePipelineBuilder{context}
      .SetPipelineName("Coordinate to distance")
      .SetPipelineLayout({descriptor_->layout()}, /*push_constant_ranges=*/{})
      .SetShader(common::file::GetVkShaderPath("aurora/coord_to_dist.comp"))
      .Build();
}

void DistanceFieldGenerator::Generate(const VkCommandBuffer& command_buffer) {
  Dispatch(command_buffer, *path_to_coord_pipeline_, kInputToPing);

  Direction direction = kPingToPong;
  for (int i = 0; i < num_steps_; ++i) {
    step_width_constant_->Flush(
        command_buffer, jump_flooding_pipeline_->layout(), /*frame=*/i,
        /*target_offset=*/0, VK_SHADER_STAGE_COMPUTE_BIT);
    Dispatch(command_buffer, *jump_flooding_pipeline_, direction);
    direction = direction == kPingToPong ? kPongToPing : kPingToPong;
  }

  // Note that the final result has to be stored in the ping image, so
  // 'direction' may need to be changed.
  if (direction == kPingToPong) {
    direction = kPingToPing;
  }
  Dispatch(command_buffer, *coord_to_dist_pipeline_, direction);
}

void DistanceFieldGenerator::Dispatch(
    const VkCommandBuffer& command_buffer,
    const Pipeline& pipeline, Direction direction) const {
  pipeline.Bind(command_buffer);
  descriptor_->PushImageInfos(
      command_buffer, pipeline.layout(), pipeline.binding_point(),
      Image::GetDescriptorTypeForLinearAccess(), image_info_maps_[direction]);
  vkCmdDispatch(command_buffer, work_group_count_.width,
                work_group_count_.height, /*groupCountZ=*/1);
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */
