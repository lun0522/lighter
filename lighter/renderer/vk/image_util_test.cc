//
//  image_util_test.cc
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/image_util.h"

#include "lighter/renderer/vk/util.h"

#ifdef ASSERT_TRUE
#undef ASSERT_TRUE
#endif

#ifdef ASSERT_FALSE
#undef ASSERT_FALSE
#endif

#include "third_party/gtest/gtest.h"

// Tests are written according to:
// https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples

namespace lighter::renderer::vk::image {
namespace {

TEST(ImageUsageTest, LinearReadInComputeShader) {
  const ImageUsage usage =
      ImageUsage::GetLinearAccessInComputeShaderUsage(AccessType::kWriteOnly);
  EXPECT_EQ(GetAccessFlags(usage), intl::AccessFlagBits::eShaderWrite);
  EXPECT_EQ(GetPipelineStageFlags(usage),
            intl::PipelineStageFlagBits::eComputeShader);
  EXPECT_EQ(GetImageLayout(usage), intl::ImageLayout::eGeneral);
}

TEST(ImageUsageTest, LinearWriteInComputeShader) {
  const ImageUsage usage =
      ImageUsage::GetLinearAccessInComputeShaderUsage(AccessType::kReadOnly);
  EXPECT_EQ(GetAccessFlags(usage), intl::AccessFlagBits::eShaderRead);
  EXPECT_EQ(GetPipelineStageFlags(usage),
            intl::PipelineStageFlagBits::eComputeShader);
  EXPECT_EQ(GetImageLayout(usage), intl::ImageLayout::eGeneral);
}

TEST(ImageUsageTest, SampleInFragmentShader) {
  const ImageUsage usage = ImageUsage::GetSampledInFragmentShaderUsage();
  EXPECT_EQ(GetAccessFlags(usage), intl::AccessFlagBits::eShaderRead);
  EXPECT_EQ(GetPipelineStageFlags(usage),
            intl::PipelineStageFlagBits::eFragmentShader);
  EXPECT_EQ(GetImageLayout(usage), intl::ImageLayout::eShaderReadOnlyOptimal);
}

}  // namespace
}  // namespace lighter::renderer::vk::image
