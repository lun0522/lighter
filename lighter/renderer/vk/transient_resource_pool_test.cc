//
//  transient_resource_pool_test.cc
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/transient_resource_pool.h"

#include <gtest/gtest.h>

#include <span>
#include <vector>

namespace lighter::renderer::vk {
namespace {

template <typename Key>
struct DummySlotEntry {
  Key key;
  PoolSlotState state;
};

TEST(TransientResourcePoolTest, FindReusablePoolSlotReusesAndAliasesImages) {
  const TransientImageKey color_key{
      .extent = {1280, 720},
      .format = intl::Format::eR8G8B8A8Unorm,
      .sample_count = intl::SampleCountFlagBits::e1,
      .usage_flags = intl::ImageUsageFlagBits::eColorAttachment |
                     intl::ImageUsageFlagBits::eSampled,
  };
  const TransientImageKey depth_key{
      .extent = {1280, 720},
      .format = intl::Format::eD32SfloatS8Uint,
      .sample_count = intl::SampleCountFlagBits::e1,
      .usage_flags = intl::ImageUsageFlagBits::eDepthStencilAttachment,
  };

  const std::vector<DummySlotEntry<TransientImageKey>> slots = {
      // Slot 0: already active across passes [0, 1] in the current frame.
      {.key = color_key,
       .state = {.in_frame_last_pass = 1, .last_used_frame = 5}},
      // Slot 1: unused so far in this frame.
      {.key = depth_key,
       .state = {.in_frame_last_pass = -1, .last_used_frame = 4}},
  };

  // Non-overlapping [2, 3] aliases onto slot 0 (1 < 2).
  const auto alias_slot =
      FindReusablePoolSlot(std::span{slots}, color_key, /*first_pass=*/2);
  ASSERT_TRUE(alias_slot.has_value());
  EXPECT_EQ(*alias_slot, 0u);

  // Overlapping [1, 3] cannot alias slot 0 because pass 1 touches both.
  EXPECT_FALSE(
      FindReusablePoolSlot(std::span{slots}, color_key, /*first_pass=*/1)
          .has_value());

  // Unused depth slot matches across frames at first_pass == 0 (-1 < 0).
  const auto depth_slot =
      FindReusablePoolSlot(std::span{slots}, depth_key, /*first_pass=*/0);
  ASSERT_TRUE(depth_slot.has_value());
  EXPECT_EQ(*depth_slot, 1u);
}

TEST(TransientResourcePoolTest, FindReusablePoolSlotMatchesBufferKeys) {
  const TransientBufferKey key_1kb{
      .size = 1024,
      .usage_flags = intl::BufferUsageFlagBits::eUniformBuffer,
  };
  const TransientBufferKey key_2kb{
      .size = 2048,
      .usage_flags = intl::BufferUsageFlagBits::eUniformBuffer,
  };

  const std::vector<DummySlotEntry<TransientBufferKey>> slots = {
      {.key = key_1kb,
       .state = {.in_frame_last_pass = 2, .last_used_frame = 3}},
  };

  EXPECT_FALSE(FindReusablePoolSlot(std::span{slots}, key_2kb, /*first_pass=*/4)
                   .has_value());
  const auto matched =
      FindReusablePoolSlot(std::span{slots}, key_1kb, /*first_pass=*/3);
  ASSERT_TRUE(matched.has_value());
  EXPECT_EQ(*matched, 0u);
}

}  // namespace
}  // namespace lighter::renderer::vk
