//
//  transient_resource_pool.h
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_TRANSIENT_RESOURCE_POOL_H
#define LIGHTER_RENDERER_VK_TRANSIENT_RESOURCE_POOL_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/vk/buffer.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/image.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk {

// Lookup key identifying compatible pooled transient images for aliasing.
struct TransientImageKey {
  intl::Extent2D extent;
  intl::Format format = intl::Format::eUndefined;
  intl::SampleCountFlagBits sample_count = intl::SampleCountFlagBits::e1;
  intl::ImageUsageFlags usage_flags;

  bool operator==(const TransientImageKey&) const = default;
};

// Lookup key identifying compatible pooled transient buffers for aliasing.
struct TransientBufferKey {
  intl::DeviceSize size = 0;
  intl::BufferUsageFlags usage_flags;

  bool operator==(const TransientBufferKey&) const = default;
};

// Tracks intra-frame pass lifetime and last-used frame index for a pooled slot.
struct PoolSlotState {
  // Last sorted execution step using this slot in the current frame (-1 if
  // unused so far in the current frame).
  int in_frame_last_pass = -1;
  // Monotonic frame counter when this slot was last acquired.
  uint64_t last_used_frame = 0;
};

// Returns the index of the first slot whose key matches 'key' and whose last
// active pass ends strictly before 'first_pass' (enabling intra-frame aliasing
// when lifetimes do not overlap).
template <typename Key, typename Entry>
std::optional<size_t> FindReusablePoolSlot(std::span<const Entry> entries,
                                           const Key& key, int first_pass) {
  for (size_t i = 0; i < entries.size(); ++i) {
    if (entries[i].key == key &&
        entries[i].state.in_frame_last_pass < first_pass) {
      return i;
    }
  }
  return std::nullopt;
}

// Pools VMA-backed transient images and buffers per in-flight frame slot,
// aliasing non-overlapping lifetimes within a frame and evicting stale entries
// across frames (e.g., after window resize).
class TransientResourcePool : public WithSharedContext {
 public:
  static constexpr uint64_t kDefaultMaxIdleFrames = 4;

  // Handle and aliasing status returned when acquiring a transient image.
  struct AcquiredImage {
    OwnedImage* image = nullptr;
    bool aliased_in_same_frame = false;
  };

  // Handle and aliasing status returned when acquiring a transient buffer.
  struct AcquiredBuffer {
    Buffer* buffer = nullptr;
    bool aliased_in_same_frame = false;
  };

  TransientResourcePool(const SharedContext& context, int num_frames_in_flight,
                        uint64_t max_idle_frames = kDefaultMaxIdleFrames);

  // This class is neither copyable nor movable.
  TransientResourcePool(const TransientResourcePool&) = delete;
  TransientResourcePool& operator=(const TransientResourcePool&) = delete;

  // Advances the frame counter, resets intra-frame aliasing ranges for
  // 'frame_index', and evicts entries unused for longer than max_idle_frames_.
  void BeginFrame(int frame_index);

  // Acquires or allocates a pooled transient image matching 'key' whose
  // previous use in this frame ended before 'lifetime.first_pass'.
  AcquiredImage AcquireImage(int frame_index, std::string_view debug_name,
                             const TransientImageKey& key,
                             ir::MultisamplingMode multisampling_mode,
                             const ir::ResourceLifetime& lifetime);

  // Acquires or allocates a pooled transient buffer matching 'key' whose
  // previous use in this frame ended before 'lifetime.first_pass'.
  AcquiredBuffer AcquireBuffer(int frame_index, const TransientBufferKey& key,
                               const ir::ResourceLifetime& lifetime);

 private:
  // Cached transient image slot paired with its key and aliasing state.
  struct ImageEntry {
    TransientImageKey key;
    PoolSlotState state;
    std::unique_ptr<OwnedImage> image;
  };

  // Cached transient buffer slot paired with its key and aliasing state.
  struct BufferEntry {
    TransientBufferKey key;
    PoolSlotState state;
    std::unique_ptr<Buffer> buffer;
  };

  // Per-frame-in-flight collection of pooled transient images and buffers.
  struct FramePool {
    std::vector<ImageEntry> images;
    std::vector<BufferEntry> buffers;
  };

  const uint64_t max_idle_frames_;
  uint64_t frame_counter_ = 0;
  std::vector<FramePool> frame_pools_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_TRANSIENT_RESOURCE_POOL_H
