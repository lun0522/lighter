//
//  type_mapping.h
//
//  Created by Pujun Lun on 12/6/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_TYPE_MAPPING_H
#define LIGHTER_RENDERER_VK_TYPE_MAPPING_H

#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk::type {

intl::VertexInputRate ConvertVertexInputRate(ir::VertexInputRate rate);

intl::Format ConvertDataFormat(ir::DataFormat format);

intl::AttachmentLoadOp ConvertAttachmentLoadOp(ir::AttachmentLoadOp op);

intl::AttachmentStoreOp ConvertAttachmentStoreOp(ir::AttachmentStoreOp op);

intl::BlendFactor ConvertBlendFactor(ir::BlendFactor factor);

intl::BlendOp ConvertBlendOp(ir::BlendOp op);

intl::CompareOp ConvertCompareOp(ir::CompareOp op);

intl::StencilOp ConvertStencilOp(ir::StencilOp op);

intl::PrimitiveTopology ConvertPrimitiveTopology(
    ir::PrimitiveTopology topology);

intl::ShaderStageFlagBits ConvertShaderStage(
    ir::shader_stage::ShaderStage stage);

intl::ShaderStageFlags ConvertShaderStages(
    ir::shader_stage::ShaderStage stages);

intl::DebugUtilsMessageSeverityFlagsEXT ConvertDebugMessageSeverities(
    uint32_t severities);

intl::DebugUtilsMessageTypeFlagsEXT ConvertDebugMessageTypes(uint32_t types);

}  // namespace lighter::renderer::vk::type

#endif  // LIGHTER_RENDERER_VK_TYPE_MAPPING_H
