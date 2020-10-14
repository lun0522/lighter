//
//  buffer.h
//
//  Created by Pujun Lun on 10/13/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_BUFFER_H
#define LIGHTER_RENDERER_BUFFER_H

#include <cstddef>
#include <vector>

#include "lighter/renderer/type.h"

namespace lighter {
namespace renderer {

class HostBuffer {
 public:
  // This class is neither copyable nor movable.
  HostBuffer(const HostBuffer&) = delete;
  HostBuffer& operator=(const HostBuffer&) = delete;

  ~HostBuffer() = default;
};

class DeviceBuffer {
 public:
  // Specifies whether the data is updated frequently.
  enum class UpdateRate { kLow, kHigh };

  // This class is neither copyable nor movable.
  DeviceBuffer(const DeviceBuffer&) = delete;
  DeviceBuffer& operator=(const DeviceBuffer&) = delete;

  ~DeviceBuffer() = default;
};

class VertexBufferView {
 public:
  // Specifies the rate at which vertex attributes are pulled from the buffer.
  enum class InputRate { kVertex, kInstance };

  // Vertex input attribute.
  struct Attribute {
    int location;
    DataFormat format;
    size_t offset;
  };

  // This class provides copy constructor and move constructor.
  VertexBufferView(VertexBufferView&&) noexcept = default;
  VertexBufferView(const VertexBufferView&) = default;

  ~VertexBufferView() = default;

  // Binds to the buffer.
  virtual void Bind(size_t offset) const = 0;
};

class UniformBufferView {};

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_BUFFER_H */
