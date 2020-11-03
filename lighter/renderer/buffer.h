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
#include "third_party/absl/types/span.h"

namespace lighter {
namespace renderer {

class HostBuffer {
 public:
  explicit HostBuffer(size_t size) { data_ = new char[size]; }

  // This class is neither copyable nor movable.
  HostBuffer(const HostBuffer&) = delete;
  HostBuffer& operator=(const HostBuffer&) = delete;

  ~HostBuffer() { delete data_; }

 private:
  // Pointer to data on the host.
  char* data_;
};

class DeviceBuffer {
 public:
  // Specifies whether the data is updated frequently.
  enum class UpdateRate { kLow, kHigh };

  // Info for copying one memory chunk of 'size' from host memory (starting from
  // 'data' pointer) to device memory with 'offset'.
  struct CopyInfo {
    const void* data;
    size_t size;
    size_t offset;
  };

  // This class is neither copyable nor movable.
  DeviceBuffer(const DeviceBuffer&) = delete;
  DeviceBuffer& operator=(const DeviceBuffer&) = delete;

  virtual ~DeviceBuffer() = default;

  // Copies data to device memory.
  virtual void CopyToDevice(absl::Span<const CopyInfo> infos) const = 0;

 protected:
  DeviceBuffer() = default;
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

  virtual ~VertexBufferView() = default;

  // Binds to this buffer.
  virtual void Bind(size_t offset) const = 0;

  // Accessors.
  InputRate input_rate() const { return input_rate_; }
  int buffer_binding() const { return buffer_binding_; }
  size_t stride() const { return stride_; }

 protected:
  VertexBufferView(InputRate input_rate, int buffer_binding, size_t stride)
      : input_rate_{input_rate}, buffer_binding_{buffer_binding},
        stride_{stride} {}

  const InputRate input_rate_;
  const int buffer_binding_;
  const size_t stride_;
};

class UniformBufferView {
 public:
  // This class provides copy constructor and move constructor.
  UniformBufferView(UniformBufferView&&) noexcept = default;
  UniformBufferView(const UniformBufferView&) = default;

  virtual ~UniformBufferView() = default;

 protected:
  UniformBufferView() = default;
};

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_BUFFER_H */
