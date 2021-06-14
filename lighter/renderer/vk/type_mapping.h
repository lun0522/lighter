//
//  type_mapping.h
//
//  Created by Pujun Lun on 12/6/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_TYPE_MAPPING_H
#define LIGHTER_RENDERER_VK_TYPE_MAPPING_H

#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk::type {

intl::VertexInputRate ConvertVertexInputRate(VertexInputRate rate);

intl::Format ConvertDataFormat(DataFormat format);

intl::AttachmentLoadOp ConvertAttachmentLoadOp(AttachmentLoadOp op);

intl::AttachmentStoreOp ConvertAttachmentStoreOp(AttachmentStoreOp op);

intl::BlendFactor ConvertBlendFactor(BlendFactor factor);

intl::BlendOp ConvertBlendOp(BlendOp op);

intl::CompareOp ConvertCompareOp(CompareOp op);

intl::StencilOp ConvertStencilOp(StencilOp op);

intl::PrimitiveTopology ConvertPrimitiveTopology(PrimitiveTopology topology);

intl::ShaderStageFlagBits ConvertShaderStage(shader_stage::ShaderStage stage);

intl::ShaderStageFlags ConvertShaderStages(shader_stage::ShaderStage stages);

intl::DebugUtilsMessageSeverityFlagsEXT ConvertDebugMessageSeverities(
    uint32_t severities);

intl::DebugUtilsMessageTypeFlagsEXT ConvertDebugMessageTypes(uint32_t types);

}  // namespace lighter::renderer::vk::type

#endif  // LIGHTER_RENDERER_VK_TYPE_MAPPING_H
