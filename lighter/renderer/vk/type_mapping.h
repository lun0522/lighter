//
//  type_mapping.h
//
//  Created by Pujun Lun on 12/6/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_TYPE_MAPPING_H
#define LIGHTER_RENDERER_VK_TYPE_MAPPING_H

#include "lighter/renderer/type.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter::renderer::vk::type {

VkVertexInputRate ConvertVertexInputRate(VertexInputRate rate);

VkFormat ConvertDataFormat(DataFormat format);

VkAttachmentLoadOp ConvertAttachmentLoadOp(AttachmentLoadOp op);

VkAttachmentStoreOp ConvertAttachmentStoreOp(AttachmentStoreOp op);

VkBlendFactor ConvertBlendFactor(BlendFactor factor);

VkBlendOp ConvertBlendOp(BlendOp op);

VkCompareOp ConvertCompareOp(CompareOp op);

VkStencilOp ConvertStencilOp(StencilOp op);

VkPrimitiveTopology ConvertPrimitiveTopology(PrimitiveTopology topology);

VkShaderStageFlagBits ConvertShaderStage(shader_stage::ShaderStage stage);

VkShaderStageFlags ConvertShaderStages(shader_stage::ShaderStage stages);

}  // namespace vk::renderer::lighter::type

#endif  // LIGHTER_RENDERER_VK_TYPE_MAPPING_H
