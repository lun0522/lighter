//
//  buffer.h
//
//  Created by Pujun Lun on 10/13/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IR_BUFFER_H
#define LIGHTER_RENDERER_IR_BUFFER_H

#include <cstddef>
#include <vector>

#include "lighter/renderer/ir/type.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer::ir {

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

 protected:
  DeviceBuffer() = default;
};

struct VertexBufferView {
  // Vertex input attribute.
  struct Attribute {
    int location;
    DataFormat format;
    size_t offset;
  };

  VertexInputRate input_rate;
  int binding_point;
  size_t stride;
  std::vector<Attribute> attributes;
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

}  // namespace lighter::renderer::ir

#endif  // LIGHTER_RENDERER_IR_BUFFER_H
