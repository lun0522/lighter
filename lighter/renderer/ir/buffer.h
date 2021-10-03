//
//  buffer.h
//
//  Created by Pujun Lun on 10/13/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IR_BUFFER_H
#define LIGHTER_RENDERER_IR_BUFFER_H

#include <vector>

#include "lighter/renderer/ir/type.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer::ir {

class Buffer {
 public:
  // Specifies whether the data is updated frequently.
  enum class UpdateRate {
    // The data is rarely or never updated.
    kLow,
    // The data is updated every frame or every few frames.
    kHigh,
  };

  // Info for copying one memory chunk of `size` from host memory (starting from
  // `data` pointer) to device memory with `offset`.
  struct CopyInfo {
    const void* data;
    size_t size;
    size_t offset;
  };

  // This class is neither copyable nor movable.
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  virtual ~Buffer() = default;

  virtual void CopyToDevice(absl::Span<const CopyInfo> copy_infos) const = 0;

 protected:
  Buffer() = default;
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
