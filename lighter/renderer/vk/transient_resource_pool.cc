//
//  transient_resource_pool.cc
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/transient_resource_pool.h"

#include <algorithm>

#include "lighter/base/util.h"

namespace lighter::renderer::vk {
namespace {

template <typename Entry>
void ResetAndEvictEntries(std::vector<Entry>* entries, uint64_t frame_counter,
                          uint64_t max_idle_frames) {
  std::erase_if(*entries, [frame_counter, max_idle_frames](const Entry& e) {
    return frame_counter > e.state.last_used_frame &&
           (frame_counter - e.state.last_used_frame) > max_idle_frames;
  });
  for (Entry& entry : *entries) {
    entry.state.in_frame_last_pass = -1;
  }
}

}  // namespace

TransientResourcePool::TransientResourcePool(const SharedContext& context,
                                             int num_frames_in_flight,
                                             uint64_t max_idle_frames)
    : WithSharedContext{context}, max_idle_frames_{max_idle_frames} {
  ASSERT_TRUE(num_frames_in_flight > 0,
              "num_frames_in_flight must be positive");
  frame_pools_.resize(num_frames_in_flight);
}

void TransientResourcePool::BeginFrame(int frame_index) {
  ++frame_counter_;
  FramePool& pool = frame_pools_.at(frame_index);
  ResetAndEvictEntries(&pool.images, frame_counter_, max_idle_frames_);
  ResetAndEvictEntries(&pool.buffers, frame_counter_, max_idle_frames_);
}

TransientResourcePool::AcquiredImage TransientResourcePool::AcquireImage(
    int frame_index, std::string_view debug_name, const TransientImageKey& key,
    ir::MultisamplingMode multisampling_mode,
    const ir::ResourceLifetime& lifetime) {
  ASSERT_TRUE(lifetime.IsUsed(),
              "Cannot acquire transient image with unused lifetime");
  FramePool& pool = frame_pools_.at(frame_index);
  const std::optional<size_t> reusable_idx = FindReusablePoolSlot(
      std::span<const ImageEntry>{pool.images}, key, lifetime.first_pass);
  if (reusable_idx.has_value()) {
    ImageEntry& entry = pool.images[*reusable_idx];
    const bool aliased = entry.state.in_frame_last_pass >= 0;
    entry.state.in_frame_last_pass = lifetime.last_pass;
    entry.state.last_used_frame = frame_counter_;
    return {.image = entry.image.get(), .aliased_in_same_frame = aliased};
  }

  auto created = OwnedImage::CreateTransient(
      context_, debug_name, util::ToVec(key.extent), key.format,
      multisampling_mode, key.usage_flags);
  OwnedImage* ptr = created.get();
  pool.images.push_back(ImageEntry{
      .key = key,
      .state = {.in_frame_last_pass = lifetime.last_pass,
                .last_used_frame = frame_counter_},
      .image = std::move(created),
  });
  return {.image = ptr, .aliased_in_same_frame = false};
}

TransientResourcePool::AcquiredBuffer TransientResourcePool::AcquireBuffer(
    int frame_index, const TransientBufferKey& key,
    const ir::ResourceLifetime& lifetime) {
  ASSERT_TRUE(lifetime.IsUsed(),
              "Cannot acquire transient buffer with unused lifetime");
  FramePool& pool = frame_pools_.at(frame_index);
  const std::optional<size_t> reusable_idx = FindReusablePoolSlot(
      std::span<const BufferEntry>{pool.buffers}, key, lifetime.first_pass);
  if (reusable_idx.has_value()) {
    BufferEntry& entry = pool.buffers[*reusable_idx];
    const bool aliased = entry.state.in_frame_last_pass >= 0;
    entry.state.in_frame_last_pass = lifetime.last_pass;
    entry.state.last_used_frame = frame_counter_;
    return {.buffer = entry.buffer.get(), .aliased_in_same_frame = aliased};
  }

  auto created = Buffer::CreateTransient(context_, key.size, key.usage_flags);
  Buffer* ptr = created.get();
  pool.buffers.push_back(BufferEntry{
      .key = key,
      .state = {.in_frame_last_pass = lifetime.last_pass,
                .last_used_frame = frame_counter_},
      .buffer = std::move(created),
  });
  return {.buffer = ptr, .aliased_in_same_frame = false};
}

}  // namespace lighter::renderer::vk
