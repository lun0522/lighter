//
//  renderer.h
//
//  Created by Pujun Lun on 10/13/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IR_RENDERER_H
#define LIGHTER_RENDERER_IR_RENDERER_H

#include <memory>
#include <string_view>
#include <vector>

#include "lighter/common/file.h"
#include "lighter/common/image.h"
#include "lighter/common/util.h"
#include "lighter/common/window.h"
#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/pass.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/type.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"

namespace lighter::renderer::ir {

class Renderer {
 public:
  // This class is neither copyable nor movable.
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  virtual ~Renderer() = default;

  // Device buffer

  virtual std::unique_ptr<Buffer> CreateBuffer(
      Buffer::UpdateRate update_rate, size_t initial_size,
      absl::Span<const BufferUsage> usages) const = 0;

  template <typename DataType>
  std::unique_ptr<Buffer> CreateBuffer(
      Buffer::UpdateRate update_rate, int num_chunks,
      absl::Span<const BufferUsage> usages) const {
    return CreateBuffer(update_rate, sizeof(DataType) * num_chunks, usages);
  }

  // Device image

  virtual const Image& GetSwapchainImage(int window_index) const = 0;

  virtual std::unique_ptr<Image> CreateColorImage(
      std::string_view name, const common::Image::Dimension& dimension,
      MultisamplingMode multisampling_mode, bool high_precision,
      absl::Span<const ImageUsage> usages) const = 0;

  virtual std::unique_ptr<Image> CreateColorImage(
      std::string_view name, const common::Image& image, bool generate_mipmaps,
      absl::Span<const ImageUsage> usages) const = 0;

  virtual std::unique_ptr<Image> CreateDepthStencilImage(
      std::string_view name, const glm::ivec2& extent,
      MultisamplingMode multisampling_mode,
      absl::Span<const ImageUsage> usages) const = 0;

  // Pass

  virtual std::unique_ptr<RenderPass> CreateRenderPass(
      RenderPassDescriptor&& descriptor) const = 0;

  virtual std::unique_ptr<ComputePass> CreateComputePass(
      ComputePassDescriptor&& descriptor) const = 0;

 protected:
  explicit Renderer(std::vector<const common::Window*>&& windows)
      : windows_{std::move(windows)} {
    for (int i = 0; i < num_windows(); ++i) {
      ASSERT_NON_NULL(windows_[i], absl::StrFormat("Window %d is nullptr", i));
    }
  }

  const std::vector<const common::Window*>& windows() const { return windows_; }
  int num_windows() const { return windows_.size(); }

 private:
  std::vector<const common::Window*> windows_;
};

}  // namespace lighter::renderer::ir

#endif  // LIGHTER_RENDERER_IR_RENDERER_H
