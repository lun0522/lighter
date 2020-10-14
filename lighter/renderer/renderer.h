//
//  renderer.h
//
//  Created by Pujun Lun on 10/13/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_RENDERER_H
#define LIGHTER_RENDERER_RENDERER_H

#include <memory>

#include "lighter/renderer/buffer.h"
#include "third_party/absl/types/span.h"

namespace lighter {
namespace renderer {

class Renderer {
 public:
  enum class Backend { kOpenGL, kVulkan };

  explicit Renderer(Backend backend) : backend_{backend} {}

  // This class is neither copyable nor movable.
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  /* Host buffer */

  std::unique_ptr<HostBuffer> CreateHostBuffer(size_t chunk_size,
                                               int num_chunks) const;

  /* Device buffer */

  std::unique_ptr<DeviceBuffer> CreateDeviceBuffer(
      DeviceBuffer::UpdateRate update_rate, size_t initial_size) const;

  /* Vertex buffer */

  VertexBufferView CreateVertexBufferView(
      int buffer_binding, VertexBufferView::InputRate input_rate,
      absl::Span<VertexBufferView::Attribute> attributes) const;

  /* Uniform buffer */

  std::unique_ptr<DeviceBuffer> CreateUniformBuffer(
      DeviceBuffer::UpdateRate update_rate,
      size_t chunk_size, int num_chunks) const {
    return CreateDeviceBuffer(update_rate, chunk_size * num_chunks);
  }

  template <typename DataType>
  std::unique_ptr<DeviceBuffer> CreateUniformBuffer(
      DeviceBuffer::UpdateRate update_rate, int num_chunks) const {
    return CreateUniformBuffer(update_rate, sizeof(DataType), num_chunks);
  }

  UniformBufferView CreateUniformBufferView(size_t chunk_size,
                                            int num_chunks) const;

  template <typename DataType>
  UniformBufferView CreateUniformBufferView(int num_chunks) const {
    return CreateUniformBufferView(sizeof(DataType), num_chunks);
  }

 private:
  const Backend backend_;
};

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_RENDERER_H */
