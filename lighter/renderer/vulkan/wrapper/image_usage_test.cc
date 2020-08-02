//
//  image_usage_test.cc
//
//  Created by Pujun Lun on 8/1/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/image_usage.h"

#include "gtest/gtest.h"
#include "third_party/vulkan/vulkan.h"

// Tests are written according to:
// https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples

namespace lighter {
namespace renderer {
namespace vulkan {
namespace image {
namespace {

using AccessType = Usage::AccessType;

TEST(ImageUsageTest, LinearReadInComputeShader) {
  const Usage usage =
      Usage::GetLinearAccessInComputeShaderUsage(AccessType::kWriteOnly);
  EXPECT_EQ(usage.GetAccessFlags(), VK_ACCESS_SHADER_WRITE_BIT);
  EXPECT_EQ(usage.GetPipelineStageFlags(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  EXPECT_EQ(usage.GetImageLayout(), VK_IMAGE_LAYOUT_GENERAL);
}

TEST(ImageUsageTest, LinearWriteInComputeShader) {
  const Usage usage =
      Usage::GetLinearAccessInComputeShaderUsage(AccessType::kReadOnly);
  EXPECT_EQ(usage.GetAccessFlags(), VK_ACCESS_SHADER_READ_BIT);
  EXPECT_EQ(usage.GetPipelineStageFlags(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  EXPECT_EQ(usage.GetImageLayout(), VK_IMAGE_LAYOUT_GENERAL);
}

TEST(ImageUsageTest, SampleInFragmentShader) {
  const Usage usage = Usage::GetSampledInFragmentShaderUsage();
  EXPECT_EQ(usage.GetAccessFlags(), VK_ACCESS_SHADER_READ_BIT);
  EXPECT_EQ(usage.GetPipelineStageFlags(),
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  EXPECT_EQ(usage.GetImageLayout(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

} /* namespace */
} /* namespace image */
} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
