//
//  renderer.cc
//
//  Created by Pujun Lun on 10/13/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/renderer.h"

#include "lighter/common/util.h"

namespace lighter {
namespace renderer {

std::unique_ptr<HostBuffer> Renderer::CreateHostBuffer(size_t chunk_size,
                                                       int num_chunks) const {
  FATAL("Not implemented yet");
}

std::unique_ptr<DeviceBuffer> Renderer::CreateDeviceBuffer(
    DeviceBuffer::UpdateRate update_rate, size_t initial_size) const {
  FATAL("Not implemented yet");
}

VertexBufferView Renderer::CreateVertexBufferView(
    int buffer_binding, VertexBufferView::InputRate input_rate,
    absl::Span<VertexBufferView::Attribute> attributes) const {
  FATAL("Not implemented yet");
}

UniformBufferView Renderer::CreateUniformBufferView(size_t chunk_size,
                                                    int num_chunks) const {
  FATAL("Not implemented yet");
}

} /* namespace renderer */
} /* namespace lighter */
