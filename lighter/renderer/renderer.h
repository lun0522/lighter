//
//  renderer.h
//
//  Created by Pujun Lun on 10/13/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_RENDERER_H
#define LIGHTER_RENDERER_RENDERER_H

#include <memory>

#include "lighter/common/file.h"
#include "lighter/renderer/buffer.h"
#include "lighter/renderer/image.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/pass.h"
#include "lighter/renderer/pipeline.h"
#include "third_party/absl/types/span.h"

namespace lighter {
namespace renderer {

class Renderer {
 public:
  // This class is neither copyable nor movable.
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  ~Renderer() = default;

  /* Host buffer */

  virtual std::unique_ptr<HostBuffer> CreateHostBuffer(size_t size) const = 0;

  template <typename DataType>
  std::unique_ptr<HostBuffer> CreateHostBuffer(int num_chunks) const {
    return CreateHostBuffer(sizeof(DataType) * num_chunks);
  }

  /* Device buffer */

  virtual std::unique_ptr<DeviceBuffer> CreateDeviceBuffer(
      DeviceBuffer::UpdateRate update_rate, size_t initial_size) const = 0;

  template <typename DataType>
  std::unique_ptr<DeviceBuffer> CreateDeviceBuffer(
      DeviceBuffer::UpdateRate update_rate, int num_chunks) const {
    return CreateDeviceBuffer(update_rate, sizeof(DataType) * num_chunks);
  }

  /* Buffer view */

  virtual VertexBufferView CreateVertexBufferView(
      const DeviceBuffer& buffer, VertexBufferView::InputRate input_rate,
      int buffer_binding, size_t stride,
      absl::Span<const VertexBufferView::Attribute> attributes) const = 0;

  virtual UniformBufferView CreateUniformBufferView(const DeviceBuffer& buffer,
                                                    size_t chunk_size,
                                                    int num_chunks) const = 0;

  template <typename DataType>
  UniformBufferView CreateUniformBufferView(const DeviceBuffer& buffer,
                                            int num_chunks) const {
    return CreateUniformBufferView(buffer, sizeof(DataType), num_chunks);
  }

  /* Pipeline */

  virtual std::unique_ptr<Pipeline> CreateGraphicsPipeline(
      const GraphicsPipelineDescriptor& descriptor) const = 0;

  virtual std::unique_ptr<Pipeline> CreateComputePipeline(
      const ComputePipelineDescriptor& descriptor) const = 0;

  /* Pass */

  virtual std::unique_ptr<GraphicsPass> CreateGraphicsPass(
      const GraphicsPassDescriptor& descriptor) const = 0;

  virtual std::unique_ptr<ComputePass> CreateComputePass(
      const ComputePassDescriptor& descriptor) const = 0;

  /* Device image */

  virtual std::unique_ptr<DeviceImage> CreateDeviceImage(
      const common::Image& image,
      absl::Span<const ImageUsage> usages) const = 0;

  virtual std::unique_ptr<DeviceImage> CreateDeviceImage(
      const common::Image& image,
      const ImageUsageHistory& usage_history) const {
    return CreateDeviceImage(image, usage_history.GetAllUsages());
  }

  virtual std::unique_ptr<DeviceImage> CreateDeviceImage(
      const common::Image::Dimension& dimension,
      MultisamplingMode multisampling_mode,
      absl::Span<const ImageUsage> usages) const = 0;

  virtual std::unique_ptr<DeviceImage> CreateDeviceImage(
      const common::Image::Dimension& dimension,
      MultisamplingMode multisampling_mode,
      const ImageUsageHistory& usage_history) const {
    return CreateDeviceImage(dimension, multisampling_mode,
                             usage_history.GetAllUsages());
  }

  /* Image view */

  virtual std::unique_ptr<SampledImageView> CreateSampledImageView(
      const DeviceImage& image,
      const SamplerDescriptor& sampler_descriptor) const = 0;
};

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_RENDERER_H */
