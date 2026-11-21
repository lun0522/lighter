//
//  image_util_test.cc
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/image_util.h"

#include "lighter/renderer/ir/type.h"

#ifdef ASSERT_TRUE
#undef ASSERT_TRUE
#endif
#ifdef ASSERT_FALSE
#undef ASSERT_FALSE
#endif
#include <gtest/gtest.h>

// Tests are written according to:
// https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples

namespace lighter::renderer::vk::image {
namespace {

using ir::AccessLocation;
using ir::AccessType;
using ir::ImageUsage;

constexpr intl::ImageSubresourceRange kColorRange{
    intl::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
constexpr intl::ImageSubresourceRange kDepthStencilRange{
    intl::ImageAspectFlagBits::eDepth | intl::ImageAspectFlagBits::eStencil, 0,
    1, 0, 1};

TEST(ImageUsageTest, LinearWriteInComputeShader) {
  const ImageUsage usage =
      ImageUsage::GetLinearAccessInComputeShaderUsage(AccessType::kWriteOnly);
  EXPECT_EQ(GetAccessFlags(usage), intl::AccessFlagBits2::eShaderWrite);
  EXPECT_EQ(GetPipelineStageFlags(usage),
            intl::PipelineStageFlagBits2::eComputeShader);
  EXPECT_EQ(GetImageLayout(usage), intl::ImageLayout::eGeneral);
}

TEST(ImageUsageTest, LinearReadInComputeShader) {
  const ImageUsage usage =
      ImageUsage::GetLinearAccessInComputeShaderUsage(AccessType::kReadOnly);
  EXPECT_EQ(GetAccessFlags(usage), intl::AccessFlagBits2::eShaderRead);
  EXPECT_EQ(GetPipelineStageFlags(usage),
            intl::PipelineStageFlagBits2::eComputeShader);
  EXPECT_EQ(GetImageLayout(usage), intl::ImageLayout::eGeneral);
}

TEST(ImageUsageTest, SampleInFragmentShader) {
  const ImageUsage usage = ImageUsage::GetSampledInFragmentShaderUsage();
  EXPECT_EQ(GetAccessFlags(usage), intl::AccessFlagBits2::eShaderRead);
  EXPECT_EQ(GetPipelineStageFlags(usage),
            intl::PipelineStageFlagBits2::eFragmentShader);
  EXPECT_EQ(GetImageLayout(usage), intl::ImageLayout::eShaderReadOnlyOptimal);
}

TEST(ImageBarrierTest, RenderTargetToSampledFragment) {
  const auto barrier = CreateImageMemoryBarrier(
      intl::Image{}, kColorRange,
      ImageUsage::GetRenderTargetUsage(/*attachment_location=*/0),
      ImageUsage::GetSampledInFragmentShaderUsage());
  ASSERT_TRUE(barrier.has_value());
  EXPECT_EQ(barrier->srcStageMask,
            intl::PipelineStageFlagBits2::eColorAttachmentOutput);
  EXPECT_EQ(barrier->srcAccessMask,
            intl::AccessFlagBits2::eColorAttachmentWrite);
  EXPECT_EQ(barrier->dstStageMask,
            intl::PipelineStageFlagBits2::eFragmentShader);
  EXPECT_EQ(barrier->dstAccessMask, intl::AccessFlagBits2::eShaderRead);
  EXPECT_EQ(barrier->oldLayout, intl::ImageLayout::eColorAttachmentOptimal);
  EXPECT_EQ(barrier->newLayout, intl::ImageLayout::eShaderReadOnlyOptimal);
}

TEST(ImageBarrierTest, ComputeStorageToFragmentRead) {
  const auto barrier = CreateImageMemoryBarrier(
      intl::Image{}, kColorRange,
      ImageUsage::GetLinearAccessInComputeShaderUsage(AccessType::kWriteOnly),
      ImageUsage::GetSampledInFragmentShaderUsage());
  ASSERT_TRUE(barrier.has_value());
  EXPECT_EQ(barrier->srcStageMask,
            intl::PipelineStageFlagBits2::eComputeShader);
  EXPECT_EQ(barrier->srcAccessMask, intl::AccessFlagBits2::eShaderWrite);
  EXPECT_EQ(barrier->dstStageMask,
            intl::PipelineStageFlagBits2::eFragmentShader);
  EXPECT_EQ(barrier->dstAccessMask, intl::AccessFlagBits2::eShaderRead);
  EXPECT_EQ(barrier->oldLayout, intl::ImageLayout::eGeneral);
  EXPECT_EQ(barrier->newLayout, intl::ImageLayout::eShaderReadOnlyOptimal);
}

TEST(ImageBarrierTest, DepthWriteToDepthReadAttachment) {
  const auto barrier = CreateImageMemoryBarrier(
      intl::Image{}, kDepthStencilRange,
      ImageUsage::GetDepthStencilUsage(AccessType::kReadWrite),
      ImageUsage::GetDepthStencilUsage(AccessType::kReadOnly));
  ASSERT_TRUE(barrier.has_value());
  EXPECT_EQ(barrier->srcStageMask,
            intl::PipelineStageFlagBits2::eEarlyFragmentTests |
                intl::PipelineStageFlagBits2::eLateFragmentTests);
  EXPECT_EQ(barrier->srcAccessMask,
            intl::AccessFlagBits2::eDepthStencilAttachmentWrite);
  EXPECT_EQ(barrier->dstStageMask,
            intl::PipelineStageFlagBits2::eEarlyFragmentTests |
                intl::PipelineStageFlagBits2::eLateFragmentTests);
  EXPECT_EQ(barrier->dstAccessMask,
            intl::AccessFlagBits2::eDepthStencilAttachmentRead);
  EXPECT_EQ(barrier->oldLayout,
            intl::ImageLayout::eDepthStencilAttachmentOptimal);
  EXPECT_EQ(barrier->newLayout,
            intl::ImageLayout::eDepthStencilReadOnlyOptimal);
}

TEST(ImageBarrierTest, DepthWriteToFragmentSampled) {
  const auto barrier = CreateImageMemoryBarrier(
      intl::Image{}, kDepthStencilRange,
      ImageUsage::GetDepthStencilUsage(AccessType::kReadWrite),
      ImageUsage::GetSampledInFragmentShaderUsage());
  ASSERT_TRUE(barrier.has_value());
  EXPECT_EQ(barrier->srcStageMask,
            intl::PipelineStageFlagBits2::eEarlyFragmentTests |
                intl::PipelineStageFlagBits2::eLateFragmentTests);
  EXPECT_EQ(barrier->srcAccessMask,
            intl::AccessFlagBits2::eDepthStencilAttachmentWrite);
  EXPECT_EQ(barrier->dstStageMask,
            intl::PipelineStageFlagBits2::eFragmentShader);
  EXPECT_EQ(barrier->dstAccessMask, intl::AccessFlagBits2::eShaderRead);
  EXPECT_EQ(barrier->oldLayout,
            intl::ImageLayout::eDepthStencilAttachmentOptimal);
  EXPECT_EQ(barrier->newLayout, intl::ImageLayout::eShaderReadOnlyOptimal);
}

TEST(ImageBarrierTest, RenderTargetToSwapchainPresent) {
  const auto barrier = CreateImageMemoryBarrier(
      intl::Image{}, kColorRange,
      ImageUsage::GetRenderTargetUsage(/*attachment_location=*/0),
      ImageUsage::GetPresentationUsage());
  ASSERT_TRUE(barrier.has_value());
  EXPECT_EQ(barrier->srcStageMask,
            intl::PipelineStageFlagBits2::eColorAttachmentOutput);
  EXPECT_EQ(barrier->srcAccessMask,
            intl::AccessFlagBits2::eColorAttachmentWrite);
  EXPECT_EQ(barrier->dstStageMask, intl::PipelineStageFlagBits2::eNone);
  EXPECT_EQ(barrier->dstAccessMask, intl::AccessFlags2{});
  EXPECT_EQ(barrier->oldLayout, intl::ImageLayout::eColorAttachmentOptimal);
  EXPECT_EQ(barrier->newLayout, intl::ImageLayout::ePresentSrcKHR);
}

TEST(ImageBarrierTest, ReadAfterReadInSameLayoutNeedsNoBarrier) {
  const auto barrier = CreateImageMemoryBarrier(
      intl::Image{}, kColorRange, ImageUsage::GetSampledInFragmentShaderUsage(),
      ImageUsage::GetSampledInFragmentShaderUsage());
  EXPECT_FALSE(barrier.has_value());
}

TEST(ImageBarrierTest, WriteAfterReadFlushesNoSrcWrites) {
  const auto barrier = CreateImageMemoryBarrier(
      intl::Image{}, kColorRange, ImageUsage::GetSampledInFragmentShaderUsage(),
      ImageUsage::GetRenderTargetUsage(/*attachment_location=*/0));
  ASSERT_TRUE(barrier.has_value());
  EXPECT_EQ(barrier->srcStageMask,
            intl::PipelineStageFlagBits2::eFragmentShader);
  EXPECT_EQ(barrier->srcAccessMask, intl::AccessFlags2{});
  EXPECT_EQ(barrier->dstStageMask,
            intl::PipelineStageFlagBits2::eColorAttachmentOutput);
  EXPECT_EQ(barrier->dstAccessMask,
            intl::AccessFlagBits2::eColorAttachmentRead |
                intl::AccessFlagBits2::eColorAttachmentWrite);
}

}  // namespace
}  // namespace lighter::renderer::vk::image
