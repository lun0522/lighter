//
//  pipeline.cc
//
//  Created by Pujun Lun on 12/5/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/pipeline.h"

#include <algorithm>
#include <vector>

#include "lighter/common/data.h"
#include "lighter/common/file.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/vk/type_mapping.h"
#include "lighter/shader_compiler/util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter::renderer::vk {
namespace {

using ir::GraphicsPipelineDescriptor;
using ir::PipelineDescriptor;

// Contains a loaded shader 'module' that will be used at 'stage'.
struct ShaderStage {
  intl::ShaderStageFlagBits stage;
  ShaderModule::RefCountedShaderModule module;
};

std::vector<intl::DescriptorSetLayout> CreateDescriptorSetLayouts() {
  FATAL("Not yet implemented");
  return {};
}

std::vector<intl::PushConstantRange> CreatePushConstantRanges(
    const PipelineDescriptor::UniformDescriptor& descriptor) {
  std::vector<intl::PushConstantRange> ranges;
  ranges.reserve(descriptor.push_constant_ranges.size());
  for (const auto& range : descriptor.push_constant_ranges) {
    ranges.push_back(intl::PushConstantRange{}
        .setStageFlags(type::ConvertShaderStages(range.shader_stages))
        .setOffset(CAST_TO_UINT(range.offset))
        .setSize(CAST_TO_UINT(range.size))
    );
  }
  return ranges;
}

intl::PipelineLayoutCreateInfo GetPipelineLayoutCreateInfo(
    const std::vector<intl::DescriptorSetLayout>* descriptor_set_layouts,
    const std::vector<intl::PushConstantRange>* push_constant_ranges) {
  return intl::PipelineLayoutCreateInfo{}
      .setSetLayouts(*descriptor_set_layouts)
      .setPushConstantRanges(*push_constant_ranges);
}

// Loads shaders in 'shader_file_path_map'.
std::vector<ShaderStage> CreateShaderStages(
    const SharedContext& context,
    const GraphicsPipelineDescriptor::ShaderPathMap& shader_path_map) {
  std::vector<ShaderStage> shader_stages;
  shader_stages.reserve(shader_path_map.size());
  for (const auto& [shader_stage, shader_path] : shader_path_map) {
    shader_stages.push_back({
        type::ConvertShaderStage(shader_stage),
        ShaderModule::RefCountedShaderModule::Get(
            /*identifier=*/shader_path, context, shader_path),
    });
  }
  return shader_stages;
}

// Extracts shader stage infos, assuming the entry point of each shader is a
// main() function.
std::vector<intl::PipelineShaderStageCreateInfo> GetShaderStageCreateInfos(
    const std::vector<ShaderStage>* shader_stages) {
  std::vector<intl::PipelineShaderStageCreateInfo> shader_stage_infos;
  shader_stage_infos.reserve(shader_stages->size());
  for (const auto& stage : *shader_stages) {
    shader_stage_infos.push_back(intl::PipelineShaderStageCreateInfo{}
        .setStage(stage.stage)
        .setModule(**stage.module)
        .setPName(shader_compiler::kShaderEntryPoint)
    );
  }
  return shader_stage_infos;
}

std::vector<intl::VertexInputBindingDescription>
CreateVertexInputBindingDescriptions(
    const GraphicsPipelineDescriptor& descriptor) {
  std::vector<intl::VertexInputBindingDescription> descriptions;
  descriptions.reserve(descriptor.vertex_buffer_views.size());
  for (const auto& view : descriptor.vertex_buffer_views) {
    descriptions.push_back(intl::VertexInputBindingDescription{}
        .setBinding(CAST_TO_UINT(view.binding_point))
        .setStride(CAST_TO_UINT(view.stride))
        .setInputRate(type::ConvertVertexInputRate(view.input_rate))
    );
  }
  return descriptions;
}

std::vector<intl::VertexInputAttributeDescription>
CreateVertexInputAttributeDescriptions(
    const GraphicsPipelineDescriptor& descriptor) {
  const auto num_attributes = common::util::Reduce<int>(
      descriptor.vertex_buffer_views,
      [](const ir::VertexBufferView& view) {
        return view.attributes.size();
      });

  std::vector<intl::VertexInputAttributeDescription> descriptions;
  descriptions.reserve(num_attributes);
  for (const auto& view : descriptor.vertex_buffer_views) {
    for (const auto& attrib : view.attributes) {
      descriptions.push_back(intl::VertexInputAttributeDescription{}
          .setLocation(CAST_TO_UINT(attrib.location))
          .setBinding(CAST_TO_UINT(view.binding_point))
          .setFormat(type::ConvertDataFormat(attrib.format))
          .setOffset(CAST_TO_UINT(attrib.offset))
      );
    }
  }
  return descriptions;
}

// Creates a vertex input state.
intl::PipelineVertexInputStateCreateInfo GetVertexInputStateCreateInfo(
    const std::vector<intl::VertexInputBindingDescription>*
        binding_descriptions,
    const std::vector<intl::VertexInputAttributeDescription>*
        attribute_descriptions) {
  return intl::PipelineVertexInputStateCreateInfo{}
      .setVertexBindingDescriptions(*binding_descriptions)
      .setVertexAttributeDescriptions(*attribute_descriptions);
}

intl::PipelineInputAssemblyStateCreateInfo GetInputAssemblyStateCreateInfo(
    const GraphicsPipelineDescriptor& descriptor) {
  return intl::PipelineInputAssemblyStateCreateInfo{}
      .setTopology(
          type::ConvertPrimitiveTopology(descriptor.primitive_topology));
}

intl::Viewport CreateViewport(const GraphicsPipelineDescriptor& descriptor) {
  const auto& viewport_info = descriptor.viewport_config.viewport;
  auto viewport = intl::Viewport{}
      .setX(viewport_info.origin.x)
      .setY(viewport_info.origin.y)
      .setWidth(viewport_info.extent.x)
      .setHeight(viewport_info.extent.y)
      .setMinDepth(0.0f)
      .setMaxDepth(1.0f);
  if (descriptor.viewport_config.flip_y) {
    viewport.y += viewport.height;
    viewport.height *= -1;
  }
  return viewport;
}

intl::Rect2D CreateScissor(const GraphicsPipelineDescriptor& descriptor) {
  const auto& scissor_info = descriptor.viewport_config.scissor;
  return intl::Rect2D{}
      .setOffset(util::ToOffset(scissor_info.origin))
      .setExtent(util::ToExtent(scissor_info.extent));
}

intl::PipelineViewportStateCreateInfo GetViewportStateCreateInfo(
    const std::vector<intl::Viewport>* viewports,
    const std::vector<intl::Rect2D>* scissors) {
  return intl::PipelineViewportStateCreateInfo{}
      .setViewports(*viewports)
      .setScissors(*scissors);
}

intl::PipelineRasterizationStateCreateInfo GetRasterizationStateCreateInfo(
    const GraphicsPipelineDescriptor& descriptor) {
  return intl::PipelineRasterizationStateCreateInfo{}
      .setCullMode(intl::CullModeFlagBits::eBack)
      .setFrontFace(
          descriptor.viewport_config.flip_y ? intl::FrontFace::eCounterClockwise
                                            : intl::FrontFace::eClockwise)
      .setLineWidth(1.0f);
}

intl::PipelineMultisampleStateCreateInfo GetMultisampleStateCreateInfo(
    intl::SampleCountFlagBits sample_count) {
  return intl::PipelineMultisampleStateCreateInfo{}
      .setRasterizationSamples(sample_count);
}

intl::StencilOpState CreateStencilOpState(
    const GraphicsPipelineDescriptor::StencilTestOneFace& test) {
  return intl::StencilOpState{}
      .setFailOp(type::ConvertStencilOp(test.stencil_fail_op))
      .setPassOp(type::ConvertStencilOp(test.stencil_and_depth_pass_op))
      .setDepthFailOp(type::ConvertStencilOp(test.stencil_pass_depth_fail_op))
      .setCompareOp(type::ConvertCompareOp(test.compare_op))
      .setCompareMask(CAST_TO_UINT(test.compare_mask))
      .setWriteMask(CAST_TO_UINT(test.write_mask))
      .setReference(CAST_TO_UINT(test.reference));
}

intl::PipelineDepthStencilStateCreateInfo GetDepthStencilStateCreateInfo(
    const GraphicsPipelineDescriptor& descriptor) {
  using StencilTest = GraphicsPipelineDescriptor::StencilTest;
  const auto& depth_test = descriptor.depth_test;
  const auto& stencil_test = descriptor.stencil_test;
  return intl::PipelineDepthStencilStateCreateInfo{}
      .setDepthTestEnable(depth_test.enable_test)
      .setDepthWriteEnable(depth_test.enable_write)
      .setDepthCompareOp(type::ConvertCompareOp(depth_test.compare_op))
      .setStencilTestEnable(stencil_test.enable_test)
      .setFront(CreateStencilOpState(
          stencil_test.tests[StencilTest::kFrontFaceIndex]))
      .setBack(CreateStencilOpState(
          stencil_test.tests[StencilTest::kBackFaceIndex]))
      .setMinDepthBounds(0.0f)
      .setMaxDepthBounds(1.0f);
}

std::vector<intl::PipelineColorBlendAttachmentState>
CreateColorBlendAttachmentStates(const GraphicsPipelineDescriptor& descriptor) {
  int max_location = -1;
  for (const auto& [location, _] : descriptor.color_attachment_map) {
    max_location = std::max(max_location, location);
  }
  std::vector<intl::PipelineColorBlendAttachmentState> color_blend_states(
      max_location + 1);
  for (const auto& [location, optional_color_blend] :
           descriptor.color_attachment_map) {
    if (!optional_color_blend.has_value()) {
      continue;
    }
    const auto& color_blend = optional_color_blend.value();
    color_blend_states[location] = intl::PipelineColorBlendAttachmentState{}
        .setSrcColorBlendFactor(type::ConvertBlendFactor(
            color_blend.src_color_blend_factor))
        .setDstColorBlendFactor(type::ConvertBlendFactor(
            color_blend.dst_color_blend_factor))
        .setColorBlendOp(type::ConvertBlendOp(
            color_blend.color_blend_op))
        .setSrcAlphaBlendFactor(type::ConvertBlendFactor(
            color_blend.src_alpha_blend_factor))
        .setDstColorBlendFactor(type::ConvertBlendFactor(
            color_blend.dst_alpha_blend_factor))
        .setAlphaBlendOp(type::ConvertBlendOp(color_blend.alpha_blend_op))
        .setColorWriteMask(intl::ColorComponentFlags{
            intl::FlagTraits<intl::ColorComponentFlagBits>::allFlags});
  }
  return color_blend_states;
}

intl::PipelineColorBlendStateCreateInfo GetColorBlendStateCreateInfo(
    const std::vector<intl::PipelineColorBlendAttachmentState>* states) {
  return intl::PipelineColorBlendStateCreateInfo{}
      .setAttachments(*states);
}

}  // namespace

ShaderModule::ShaderModule(const SharedContext& context,
                           std::string_view file_path)
    : WithSharedContext{context} {
  const common::Data file_data = common::file::LoadDataFromFile(file_path);
  const auto shader_module_create_info = intl::ShaderModuleCreateInfo{}
      .setCodeSize(file_data.size())
      .setPCode(file_data.data<uint32_t>());
  shader_module_ = vk_device().createShaderModule(shader_module_create_info,
                                                  vk_host_allocator());
}

Pipeline::Pipeline(const SharedContext& context,
                   const GraphicsPipelineDescriptor& descriptor,
                   intl::SampleCountFlagBits sample_count,
                   intl::RenderPass render_pass, int subpass_index)
    : Pipeline{context, descriptor.pipeline_name,
               intl::PipelineBindPoint::eGraphics,
               descriptor.uniform_descriptor} {
  const auto shader_stages = CreateShaderStages(context_,
                                                descriptor.shader_path_map);
  const auto shader_stage_create_infos =
      GetShaderStageCreateInfos(&shader_stages);

  const auto vertex_input_binding_descs =
      CreateVertexInputBindingDescriptions(descriptor);
  const auto vertex_input_attribute_descs =
      CreateVertexInputAttributeDescriptions(descriptor);
  const auto vertex_input_state_create_info = GetVertexInputStateCreateInfo(
      &vertex_input_binding_descs, &vertex_input_attribute_descs);

  const std::vector<intl::Viewport> viewports{CreateViewport(descriptor)};
  const std::vector<intl::Rect2D> scissors{CreateScissor(descriptor)};
  const auto viewport_state_create_info =
      GetViewportStateCreateInfo(&viewports, &scissors);

  const auto color_blend_attachment_states =
      CreateColorBlendAttachmentStates(descriptor);
  const auto color_blend_state_create_info =
      GetColorBlendStateCreateInfo(&color_blend_attachment_states);

  const auto input_assembly_state_create_info =
      GetInputAssemblyStateCreateInfo(descriptor);
  const auto rasterization_state_create_info =
      GetRasterizationStateCreateInfo(descriptor);
  const auto multisample_state_create_info =
      GetMultisampleStateCreateInfo(sample_count);
  const auto depth_stencil_state_create_info =
      GetDepthStencilStateCreateInfo(descriptor);

  const auto pipeline_create_info = intl::GraphicsPipelineCreateInfo{}
      .setStages(shader_stage_create_infos)
      .setPVertexInputState(&vertex_input_state_create_info)
      .setPInputAssemblyState(&input_assembly_state_create_info)
      .setPViewportState(&viewport_state_create_info)
      .setPRasterizationState(&rasterization_state_create_info)
      .setPMultisampleState(&multisample_state_create_info)
      .setPDepthStencilState(&depth_stencil_state_create_info)
      .setPColorBlendState(&color_blend_state_create_info)
      .setLayout(pipeline_layout_)
      .setRenderPass(render_pass)
      .setSubpass(CAST_TO_UINT(subpass_index));
  const auto [_, pipeline_] = vk_device().createGraphicsPipeline(
      intl::PipelineCache{}, pipeline_create_info, vk_host_allocator());
}

Pipeline::Pipeline(const SharedContext& context,
                   const ir::ComputePipelineDescriptor& descriptor)
    : Pipeline{context, descriptor.pipeline_name,
               intl::PipelineBindPoint::eCompute,
               descriptor.uniform_descriptor} {
  const auto shader_stages = CreateShaderStages(
      context_, {{ir::shader_stage::COMPUTE, descriptor.shader_path}});
  const auto shader_stage_create_infos =
      GetShaderStageCreateInfos(&shader_stages);

  const auto pipeline_create_info = intl::ComputePipelineCreateInfo{}
      .setStage(shader_stage_create_infos[0])
      .setLayout(pipeline_layout_);
  const auto [_, pipeline_] = vk_device().createComputePipeline(
      intl::PipelineCache{}, pipeline_create_info, vk_host_allocator());
}

Pipeline::Pipeline(
    const SharedContext& context, std::string_view name,
    intl::PipelineBindPoint binding_point,
    const PipelineDescriptor::UniformDescriptor& uniform_descriptor)
    : WithSharedContext{context}, name_{name},
      binding_point_{binding_point} {
  const auto descriptor_set_layouts = CreateDescriptorSetLayouts();
  const auto push_constant_ranges =
      CreatePushConstantRanges(uniform_descriptor);

  const auto layout_create_info = GetPipelineLayoutCreateInfo(
      &descriptor_set_layouts, &push_constant_ranges);
  pipeline_layout_ = vk_device().createPipelineLayout(layout_create_info,
                                                      vk_host_allocator());
}

void Pipeline::Bind(intl::CommandBuffer command_buffer) const {
  command_buffer.bindPipeline(binding_point_, pipeline_);
}

Pipeline::~Pipeline() {
  context_->DeviceDestroy(pipeline_);
  context_->DeviceDestroy(pipeline_layout_);
#ifndef NDEBUG
  LOG_INFO << absl::StreamFormat("Pipeline '%s' destructed", name_);
#endif  // DEBUG
}

}  // namespace lighter::renderer::vk
