//
//  pipeline.cc
//
//  Created by Pujun Lun on 12/5/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/pipeline.h"

#include <vector>

#include "lighter/renderer/vk/type_mapping.h"
#include "lighter/renderer/vk/util.h"

namespace lighter {
namespace renderer {
namespace vk {
namespace {

std::vector<VkVertexInputBindingDescription>
CreateVertexInputBindingDescriptions(
    const GraphicsPipelineDescriptor& descriptor) {
  std::vector<VkVertexInputBindingDescription> descriptions;
  descriptions.reserve(descriptor.vertex_buffer_views.size());
  for (const auto& view : descriptor.vertex_buffer_views) {
    descriptions.push_back(VkVertexInputBindingDescription{
        CAST_TO_UINT(view.binding_point),
        CAST_TO_UINT(view.stride),
        type::ConvertVertexInputRate(view.input_rate),
    });
  }
  return descriptions;
}

std::vector<VkVertexInputAttributeDescription>
CreateVertexInputAttributeDescriptions(
    const GraphicsPipelineDescriptor& descriptor) {
  const auto num_attributes = common::util::Reduce<int>(
      descriptor.vertex_buffer_views,
      [](const renderer::VertexBufferView& view) {
        return view.attributes.size();
      });

  std::vector<VkVertexInputAttributeDescription> descriptions;
  descriptions.reserve(num_attributes);
  for (const auto& view : descriptor.vertex_buffer_views) {
    for (const auto& attrib : view.attributes) {
      descriptions.push_back(VkVertexInputAttributeDescription{
          CAST_TO_UINT(attrib.location),
          CAST_TO_UINT(view.binding_point),
          type::ConvertDataFormat(attrib.format),
          CAST_TO_UINT(attrib.offset),
      });
    }
  }
  return descriptions;
}

// Creates a vertex input state.
VkPipelineVertexInputStateCreateInfo CreateVertexInputInfo(
    const std::vector<VkVertexInputBindingDescription>* binding_descriptions,
    const std::vector<VkVertexInputAttributeDescription>*
        attribute_descriptions) {
  return {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      CONTAINER_SIZE(*binding_descriptions),
      binding_descriptions->data(),
      CONTAINER_SIZE(*attribute_descriptions),
      attribute_descriptions->data(),
  };
}

VkPipelineInputAssemblyStateCreateInfo CreateInputAssemblyInfo(
    const GraphicsPipelineDescriptor& descriptor) {
  return {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      type::ConvertPrimitiveTopology(descriptor.primitive_topology),
      // 'primitiveRestartEnable' matters for drawing line/triangle strips.
      /*primitiveRestartEnable=*/VK_FALSE,
  };
}

VkViewport CreateViewport(const GraphicsPipelineDescriptor& descriptor) {
  VkViewport viewport{
      descriptor.viewport.origin.x,
      descriptor.viewport.origin.y,
      descriptor.viewport.extent.x,
      descriptor.viewport.extent.y,
      /*minDepth=*/0.0f,
      /*maxDepth=*/1.0f,
  };
  if (descriptor.flip_y) {
    viewport.y += viewport.height;
    viewport.height *= -1;
  }
  return viewport;
}

VkRect2D CreateScissor(const GraphicsPipelineDescriptor& descriptor) {
  return {util::CreateOffset(descriptor.scissor.origin),
          util::CreateExtent(descriptor.scissor.extent)};
}

VkPipelineViewportStateCreateInfo CreateViewportInfo(const VkViewport* viewport,
                                                     const VkRect2D* scissor) {
  return {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      /*viewportCount=*/1,
      viewport,
      /*scissorCount=*/1,
      scissor,
  };
}

VkPipelineRasterizationStateCreateInfo CreateRasterizationInfo(
    const GraphicsPipelineDescriptor& descriptor) {
  return {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      // If false, fragments beyond clip space will be discarded, not clamped.
      /*depthClampEnable=*/VK_FALSE,
      // If true, disable outputs to the framebuffer.
      /*rasterizerDiscardEnable=*/VK_FALSE,
      // Fill polygons with fragments.
      VK_POLYGON_MODE_FILL,
      VK_CULL_MODE_BACK_BIT,
      descriptor.flip_y ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                        : VK_FRONT_FACE_CLOCKWISE,
      // Whether to let the rasterizer alter depth values.
      /*depthBiasEnable=*/VK_FALSE,
      /*depthBiasConstantFactor=*/0.0f,
      /*depthBiasClamp=*/0.0f,
      /*depthBiasSlopeFactor=*/0.0f,
      /*lineWidth=*/1.0f,
  };
}

} /* namespace */

Pipeline::Pipeline(SharedContext context,
                   const GraphicsPipelineDescriptor& descriptor)
    : Pipeline{std::move(context), descriptor.pipeline_name,
               VK_PIPELINE_BIND_POINT_GRAPHICS} {
  const auto vertex_input_bindings =
      CreateVertexInputBindingDescriptions(descriptor);
  const auto vertex_input_attributes =
      CreateVertexInputAttributeDescriptions(descriptor);
  const auto vertex_input_info = CreateVertexInputInfo(
      &vertex_input_bindings, &vertex_input_attributes);

  const auto viewport = CreateViewport(descriptor);
  const auto scissor = CreateScissor(descriptor);
  const auto viewport_info = CreateViewportInfo(&viewport, &scissor);

  const auto input_assembly_info = CreateInputAssemblyInfo(descriptor);
  const auto rasterization_info = CreateRasterizationInfo(descriptor);
}

Pipeline::Pipeline(SharedContext context,
                   const ComputePipelineDescriptor& descriptor)
    : Pipeline{std::move(context), descriptor.pipeline_name,
               VK_PIPELINE_BIND_POINT_COMPUTE} {
  FATAL("Not implemented yet");
}

void Pipeline::Bind(const VkCommandBuffer& command_buffer) const {
  vkCmdBindPipeline(command_buffer, binding_point_, pipeline_);
}

Pipeline::~Pipeline() {
  vkDestroyPipeline(*context_->device(), pipeline_,
                    *context_->host_allocator());
  vkDestroyPipelineLayout(*context_->device(), layout_,
                          *context_->host_allocator());
#ifndef NDEBUG
  LOG_INFO << absl::StreamFormat("Pipeline '%s' destructed", name());
#endif  /* !NDEBUG */
}

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
