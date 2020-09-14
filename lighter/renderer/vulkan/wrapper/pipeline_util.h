//
//  pipeline_util.h
//
//  Created by Pujun Lun on 6/19/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_WRAPPER_PIPELINE_UTIL_H
#define LIGHTER_RENDERER_VULKAN_WRAPPER_PIPELINE_UTIL_H

#include <vector>

#include "lighter/common/file.h"
#include "lighter/renderer/vulkan/wrapper/buffer.h"
#include "lighter/renderer/vulkan/wrapper/pipeline.h"
#include "lighter/renderer/vulkan/wrapper/util.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace pipeline {

/** Stencil test **/

// Returns a read-only stencil op state. The value stored in stencil buffer will
// be compared with 'reference' value using 'compare_op' to determine whether
// the stencil test passes.
VkStencilOpState GetStencilReadOpState(VkCompareOp compare_op,
                                       uint32_t reference);

// Returns a write-only stencil op state. The stencil test will always pass, and
// write 'reference' value to the stencil attachment.
VkStencilOpState GetStencilWriteOpState(uint32_t reference);

/** Viewport **/

// Returns a viewport transform targeting the full frame of 'frame_size'.
GraphicsPipelineBuilder::ViewportInfo GetFullFrameViewport(
    const VkExtent2D& frame_size);

// Returns a viewport transform that keeps the aspect ratio of objects
// unchanged, and fills the frame as much as possible.
GraphicsPipelineBuilder::ViewportInfo GetViewport(const VkExtent2D& frame_size,
                                                  float aspect_ratio);

/** Color blend **/

// Returns the blend state that simply adds up source and destination colors.
// This is used for single channel images that do not have alpha channels.
VkPipelineColorBlendAttachmentState GetColorBlendState(bool enable_blend);

// Returns the blend state that gives:
//   C = Cs * As + Cd * (1. - As)
//   A = 1. * As + Ad * (1. - As)
// Where: C - color, A - alpha, s - source, d - destination.
VkPipelineColorBlendAttachmentState GetColorAlphaBlendState(bool enable_blend);

/** Vertex input binding **/

// Returns how to interpret the vertex data. Note that the 'binding' field of
// the returned value will not be set, since it will be assigned in pipeline.
VkVertexInputBindingDescription GetBindingDescription(uint32_t stride,
                                                      bool instancing);

// Convenience function to return VkVertexInputBindingDescription, assuming
// each vertex will get data of DataType, which is updated per-vertex.
template <typename DataType>
inline VkVertexInputBindingDescription GetPerVertexBindingDescription() {
  return GetBindingDescription(sizeof(DataType), /*instancing=*/false);
}

// Convenience function to return VkVertexInputBindingDescription, assuming
// each vertex will get data of DataType, which is updated per-instance.
template <typename DataType>
inline VkVertexInputBindingDescription GetPerInstanceBindingDescription() {
  return GetBindingDescription(sizeof(DataType), /*instancing=*/true);
}

/** Vertex input attribute **/

// Converts common::VertexAttribute to VertexBuffer::Attribute.
std::vector<VertexBuffer::Attribute> ConvertVertexAttributes(
    absl::Span<const common::VertexAttribute> attributes);

// Convenience function to return a vector of VertexBuffer::Attribute, assuming
// the class of DataType provides a static GetVertexAttributes() method.
template <typename DataType>
std::vector<VertexBuffer::Attribute> GetVertexAttributes() {
  return ConvertVertexAttributes(DataType::GetVertexAttributes());
}

} /* namespace pipeline */
} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_WRAPPER_PIPELINE_UTIL_H */
