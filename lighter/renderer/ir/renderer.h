//
//  renderer.h
//
//  Created by Pujun Lun on 10/13/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IR_RENDERER_H
#define LIGHTER_RENDERER_IR_RENDERER_H

#include <format>
#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "lighter/common/image.h"
#include "lighter/common/util.h"
#include "lighter/common/window.h"
#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/type.h"

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
      std::span<const BufferUsage> usages) const = 0;

  template <typename DataType>
  std::unique_ptr<Buffer> CreateBuffer(
      Buffer::UpdateRate update_rate, int num_chunks,
      std::span<const BufferUsage> usages) const {
    return CreateBuffer(update_rate, sizeof(DataType) * num_chunks, usages);
  }

  // Device image

  virtual std::unique_ptr<Image> CreateColorImage(
      std::string_view name, const common::Image::Dimension& dimension,
      MultisamplingMode multisampling_mode, bool high_precision,
      std::span<const ImageUsage> usages) const = 0;

  virtual std::unique_ptr<Image> CreateColorImage(
      std::string_view name, const Image& match_target,
      MultisamplingMode multisampling_mode,
      std::span<const ImageUsage> usages) const = 0;

  virtual std::unique_ptr<Image> CreateColorImage(
      std::string_view name, const common::Image& image, bool generate_mipmaps,
      std::span<const ImageUsage> usages) const = 0;

  virtual std::unique_ptr<Image> CreateDepthStencilImage(
      std::string_view name, const glm::ivec2& extent,
      MultisamplingMode multisampling_mode,
      std::span<const ImageUsage> usages) const = 0;

  // Frame Graph execution

  // Compiles and executes 'graph' for one frame to 'window_index', returning
  // false if the swapchain was recreated during this call.
  virtual bool RenderFrame(int window_index, RenderGraph& graph) = 0;

  // Compiles and executes an offscreen 'graph' synchronously via a one-time
  // graphics submission (used for initialization-time baking).
  virtual void ExecuteRenderGraph(RenderGraph& graph) = 0;

  // Blocks until all queued GPU work completes.
  virtual void WaitIdle() = 0;

 protected:
  explicit Renderer(std::vector<common::Window*>&& windows)
      : windows_{std::move(windows)} {
    for (int i = 0; i < num_windows(); ++i) {
      ASSERT_NON_NULL(windows_[i], std::format("Window {} is nullptr", i));
    }
  }

  common::Window& mutable_window(int index) { return *windows_.at(index); }
  const std::vector<common::Window*>& windows() const { return windows_; }
  int num_windows() const { return windows_.size(); }

 private:
  std::vector<common::Window*> windows_;
};

}  // namespace lighter::renderer::ir

#endif  // LIGHTER_RENDERER_IR_RENDERER_H
