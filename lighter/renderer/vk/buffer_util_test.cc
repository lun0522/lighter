//
//  buffer_util_test.cc
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/buffer_util.h"

#include "lighter/renderer/ir/type.h"

#ifdef ASSERT_TRUE
#undef ASSERT_TRUE
#endif
#ifdef ASSERT_FALSE
#undef ASSERT_FALSE
#endif
#include <gtest/gtest.h>

namespace lighter::renderer::vk::buffer {
namespace {

using ir::AccessLocation;
using ir::BufferUsage;

TEST(BufferBarrierTest, TransferWriteToVertexInput) {
  const auto barrier = CreateBufferMemoryBarrier(
      intl::Buffer{}, /*offset=*/0, VK_WHOLE_SIZE,
      BufferUsage::GetTransferDestinationUsage(),
      BufferUsage::GetVertexBufferUsage(BufferUsage::UsageType::kVertexOnly));
  ASSERT_TRUE(barrier.has_value());
  EXPECT_EQ(barrier->srcStageMask, intl::PipelineStageFlagBits2::eTransfer);
  EXPECT_EQ(barrier->srcAccessMask, intl::AccessFlagBits2::eTransferWrite);
  EXPECT_EQ(barrier->dstStageMask, intl::PipelineStageFlagBits2::eVertexInput);
  EXPECT_EQ(barrier->dstAccessMask,
            intl::AccessFlagBits2::eVertexAttributeRead);
}

TEST(BufferBarrierTest, TransferWriteToUniformCompute) {
  const auto barrier = CreateBufferMemoryBarrier(
      intl::Buffer{}, /*offset=*/0, VK_WHOLE_SIZE,
      BufferUsage::GetTransferDestinationUsage(),
      BufferUsage::GetUniformBufferUsage(AccessLocation::kComputeShader));
  ASSERT_TRUE(barrier.has_value());
  EXPECT_EQ(barrier->srcStageMask, intl::PipelineStageFlagBits2::eTransfer);
  EXPECT_EQ(barrier->srcAccessMask, intl::AccessFlagBits2::eTransferWrite);
  EXPECT_EQ(barrier->dstStageMask,
            intl::PipelineStageFlagBits2::eComputeShader);
  EXPECT_EQ(barrier->dstAccessMask, intl::AccessFlagBits2::eUniformRead);
}

TEST(BufferBarrierTest, ReadAfterReadNeedsNoBarrier) {
  const auto barrier = CreateBufferMemoryBarrier(
      intl::Buffer{}, /*offset=*/0, VK_WHOLE_SIZE,
      BufferUsage::GetVertexBufferUsage(BufferUsage::UsageType::kVertexOnly),
      BufferUsage::GetUniformBufferUsage(AccessLocation::kFragmentShader));
  EXPECT_FALSE(barrier.has_value());
}

}  // namespace
}  // namespace lighter::renderer::vk::buffer
