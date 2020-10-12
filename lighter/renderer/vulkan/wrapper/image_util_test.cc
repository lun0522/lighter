//
//  image_util_test.cc
//
//  Created by Pujun Lun on 8/1/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/image_util.h"

#include "gtest/gtest.h"
#include "third_party/vulkan/vulkan.h"

// Tests are written according to:
// https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples

namespace lighter {
namespace renderer {
namespace vulkan {
namespace image {
namespace {

using AccessType = ImageUsage::AccessType;

TEST(ImageUsageTest, LinearReadInComputeShader) {
  const ImageUsage usage =
      ImageUsage::GetLinearAccessInComputeShaderUsage(AccessType::kWriteOnly);
  EXPECT_EQ(GetAccessFlags(usage), VK_ACCESS_SHADER_WRITE_BIT);
  EXPECT_EQ(GetPipelineStageFlags(usage), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  EXPECT_EQ(GetImageLayout(usage), VK_IMAGE_LAYOUT_GENERAL);
}

TEST(ImageUsageTest, LinearWriteInComputeShader) {
  const ImageUsage usage =
      ImageUsage::GetLinearAccessInComputeShaderUsage(AccessType::kReadOnly);
  EXPECT_EQ(GetAccessFlags(usage), VK_ACCESS_SHADER_READ_BIT);
  EXPECT_EQ(GetPipelineStageFlags(usage), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  EXPECT_EQ(GetImageLayout(usage), VK_IMAGE_LAYOUT_GENERAL);
}

TEST(ImageUsageTest, SampleInFragmentShader) {
  const ImageUsage usage = ImageUsage::GetSampledInFragmentShaderUsage();
  EXPECT_EQ(GetAccessFlags(usage), VK_ACCESS_SHADER_READ_BIT);
  EXPECT_EQ(GetPipelineStageFlags(usage),
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
  EXPECT_EQ(GetImageLayout(usage), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

} /* namespace */
} /* namespace image */
} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
