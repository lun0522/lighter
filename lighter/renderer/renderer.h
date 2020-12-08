//
//  renderer.h
//
//  Created by Pujun Lun on 10/13/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_RENDERER_H
#define LIGHTER_RENDERER_RENDERER_H

#include <memory>
#include <vector>

#include "lighter/common/file.h"
#include "lighter/common/image.h"
#include "lighter/common/util.h"
#include "lighter/common/window.h"
#include "lighter/renderer/buffer.h"
#include "lighter/renderer/buffer_usage.h"
#include "lighter/renderer/image.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/pass.h"
#include "lighter/renderer/pipeline.h"
#include "lighter/renderer/type.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"

namespace lighter {
namespace renderer {

class Renderer {
 public:
  struct WindowConfig {
    const common::Window* window;
    MultisamplingMode multisampling_mode;
  };

  // This class is neither copyable nor movable.
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  virtual ~Renderer() = default;

  /* Host buffer */

  std::unique_ptr<HostBuffer> CreateHostBuffer(size_t size) const {
    return absl::make_unique<HostBuffer>(size);
  }

  template <typename DataType>
  std::unique_ptr<HostBuffer> CreateHostBuffer(int num_chunks) const {
    return CreateHostBuffer(sizeof(DataType) * num_chunks);
  }

  /* Device buffer */

  virtual std::unique_ptr<DeviceBuffer> CreateDeviceBuffer(
      DeviceBuffer::UpdateRate update_rate, size_t initial_size,
      absl::Span<const BufferUsage> usages) const = 0;

  template <typename DataType>
  std::unique_ptr<DeviceBuffer> CreateDeviceBuffer(
      DeviceBuffer::UpdateRate update_rate, int num_chunks,
      absl::Span<const BufferUsage> usages) const {
    return CreateDeviceBuffer(update_rate, sizeof(DataType) * num_chunks,
                              usages);
  }

  /* Device image */

  virtual std::unique_ptr<DeviceImage> CreateColorImage(
      const common::Image::Dimension& dimension,
      MultisamplingMode multisampling_mode,
      absl::Span<const ImageUsage> usages) const = 0;

  virtual std::unique_ptr<DeviceImage> CreateColorImage(
      const common::Image& image, bool generate_mipmaps,
      absl::Span<const ImageUsage> usages) const = 0;

  virtual std::unique_ptr<DeviceImage> CreateDepthStencilImage(
      const glm::ivec2& extent, MultisamplingMode multisampling_mode,
      absl::Span<const ImageUsage> usages) const = 0;

  /* Pass */

  virtual std::unique_ptr<GraphicsPass> CreateGraphicsPass(
      const GraphicsPassDescriptor& descriptor) const = 0;

  virtual std::unique_ptr<ComputePass> CreateComputePass(
      const ComputePassDescriptor& descriptor) const = 0;

 protected:
  explicit Renderer(std::vector<WindowConfig>&& window_configs)
      : window_configs_{std::move(window_configs)} {}

  // Returns pointers to windows that are being rendered to.
  std::vector<const common::Window*> GetWindows() const {
    return common::util::TransformToVector<WindowConfig, const common::Window*>(
        window_configs_,
        [](const WindowConfig& config) { return config.window; });
  }

  // Accessors.
  const std::vector<WindowConfig>& window_configs() const {
    return window_configs_;
  }

 private:
  const std::vector<WindowConfig> window_configs_;
};

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_RENDERER_H */
