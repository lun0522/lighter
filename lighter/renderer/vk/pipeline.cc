//
//  pipeline.cc
//
//  Created by Pujun Lun on 12/5/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/pipeline.h"

#include <algorithm>
#include <array>
#include <format>
#include <map>
#include <spirv_cross/spirv_cross.hpp>
#include <utility>
#include <vector>

#include "lighter/common/data.h"
#include "lighter/common/file.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/vk/type_mapping.h"
#include "lighter/shader_compiler/util.h"

namespace lighter::renderer::vk {
namespace {

using ir::GraphicsPipelineDescriptor;
using ir::PipelineDescriptor;

// Contains a loaded shader 'module' that will be used at 'stage'.
struct ShaderStage {
  intl::ShaderStageFlagBits stage;
  ShaderModule::RefCountedShaderModule module;
};

intl::ShaderStageFlagBits ConvertExecutionModel(spv::ExecutionModel model) {
  switch (model) {
    case spv::ExecutionModelVertex:
      return intl::ShaderStageFlagBits::eVertex;
    case spv::ExecutionModelFragment:
      return intl::ShaderStageFlagBits::eFragment;
    case spv::ExecutionModelGLCompute:
      return intl::ShaderStageFlagBits::eCompute;
    default:
      FATAL("Unsupported SPIR-V execution model");
  }
}

void AppendReflectedResources(
    const spirv_cross::Compiler& compiler,
    const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
    intl::DescriptorType descriptor_type, intl::ShaderStageFlagBits stage,
    std::vector<ReflectedBinding>* out_bindings) {
  for (const spirv_cross::Resource& res : resources) {
    const uint32_t set =
        compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
    const uint32_t binding =
        compiler.get_decoration(res.id, spv::DecorationBinding);
    uint32_t descriptor_count = 1;
    const auto& type = compiler.get_type(res.type_id);
    for (size_t i = 0; i < type.array.size(); ++i) {
      ASSERT_TRUE(
          type.array_size_literal[i],
          "Specialization-constant descriptor array size is not supported");
      descriptor_count *= type.array[i];
    }
    std::string name = res.name;
    if (name.empty()) {
      name = compiler.get_name(res.id);
    }
    out_bindings->push_back(ReflectedBinding{
        .set = set,
        .binding = binding,
        .descriptor_type = descriptor_type,
        .descriptor_count = descriptor_count,
        .stage_flags = stage,
        .name = std::move(name),
    });
  }
}

const ReflectedBinding* FindReflectedBinding(
    const PipelineReflection& reflection, uint32_t set, uint32_t binding) {
  for (const auto& entry : reflection.bindings) {
    if (entry.set == set && entry.binding == binding) {
      return &entry;
    }
  }
  return nullptr;
}

bool IsBufferDescriptorType(intl::DescriptorType type) {
  return type == intl::DescriptorType::eUniformBuffer ||
         type == intl::DescriptorType::eStorageBuffer ||
         type == intl::DescriptorType::eUniformBufferDynamic ||
         type == intl::DescriptorType::eStorageBufferDynamic;
}

bool IsImageDescriptorType(intl::DescriptorType type) {
  return type == intl::DescriptorType::eCombinedImageSampler ||
         type == intl::DescriptorType::eSampledImage ||
         type == intl::DescriptorType::eStorageImage ||
         type == intl::DescriptorType::eInputAttachment;
}

std::vector<intl::DescriptorSetLayout> CreateDescriptorSetLayouts(
    const Context& context, const PipelineReflection& reflection) {
  if (reflection.bindings.empty()) {
    return {};
  }

  uint32_t max_set = 0;
  for (const auto& binding : reflection.bindings) {
    max_set = std::max(max_set, binding.set);
  }

  std::vector<std::vector<intl::DescriptorSetLayoutBinding>> bindings_per_set(
      max_set + 1);
  for (const auto& binding : reflection.bindings) {
    bindings_per_set[binding.set].push_back(
        intl::DescriptorSetLayoutBinding{}
            .setBinding(binding.binding)
            .setDescriptorType(binding.descriptor_type)
            .setDescriptorCount(binding.descriptor_count)
            .setStageFlags(binding.stage_flags));
  }

  std::vector<intl::DescriptorSetLayout> layouts;
  layouts.reserve(bindings_per_set.size());
  for (size_t set_idx = 0; set_idx < bindings_per_set.size(); ++set_idx) {
    intl::DescriptorSetLayoutCreateFlags flags;
    if (set_idx == 0 && !bindings_per_set[set_idx].empty()) {
      flags |= intl::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR;
    }
    const auto create_info =
        intl::DescriptorSetLayoutCreateInfo{}.setFlags(flags).setBindings(
            bindings_per_set[set_idx]);
    layouts.push_back(context.device()->createDescriptorSetLayout(
        create_info, *context.host_allocator()));
  }
  return layouts;
}

std::vector<intl::PushConstantRange> CreatePushConstantRanges(
    std::span<const ReflectedPushConstantRange> reflected_ranges) {
  std::vector<intl::PushConstantRange> ranges;
  ranges.reserve(reflected_ranges.size());
  for (const auto& range : reflected_ranges) {
    ranges.push_back(intl::PushConstantRange{}
                         .setStageFlags(range.stage_flags)
                         .setOffset(range.offset)
                         .setSize(range.size));
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

// Loads shaders in 'shader_path_map'.
std::vector<ShaderStage> CreateShaderStages(
    const SharedContext& context,
    const GraphicsPipelineDescriptor::ShaderPathMap& shader_path_map) {
  std::vector<ShaderStage> shader_stages;
  shader_stages.reserve(shader_path_map.size());
  for (const auto& [shader_stage, shader_path] : shader_path_map) {
    const intl::ShaderStageFlagBits vk_stage =
        type::ConvertShaderStage(shader_stage);
    auto module = ShaderModule::RefCountedShaderModule::Get(
        /*identifier=*/shader_path, context, shader_path);
    ASSERT_TRUE((*module).stage() == vk_stage,
                std::format("Shader '{}' stage mismatch", shader_path));
    shader_stages.push_back({vk_stage, std::move(module)});
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
    shader_stage_infos.push_back(
        intl::PipelineShaderStageCreateInfo{}
            .setStage(stage.stage)
            .setModule(**stage.module)
            .setPName(shader_compiler::kShaderEntryPoint));
  }
  return shader_stage_infos;
}

std::vector<intl::VertexInputBindingDescription>
CreateVertexInputBindingDescriptions(
    const GraphicsPipelineDescriptor& descriptor) {
  std::vector<intl::VertexInputBindingDescription> descriptions;
  descriptions.reserve(descriptor.vertex_buffer_views.size());
  for (const auto& view : descriptor.vertex_buffer_views) {
    descriptions.push_back(
        intl::VertexInputBindingDescription{}
            .setBinding(CAST_TO_UINT(view.binding_point))
            .setStride(CAST_TO_UINT(view.stride))
            .setInputRate(type::ConvertVertexInputRate(view.input_rate)));
  }
  return descriptions;
}

std::vector<intl::VertexInputAttributeDescription>
CreateVertexInputAttributeDescriptions(
    const GraphicsPipelineDescriptor& descriptor) {
  const auto num_attributes = common::util::Reduce<int>(
      descriptor.vertex_buffer_views,
      [](const ir::VertexBufferView& view) { return view.attributes.size(); });

  std::vector<intl::VertexInputAttributeDescription> descriptions;
  descriptions.reserve(num_attributes);
  for (const auto& view : descriptor.vertex_buffer_views) {
    for (const auto& attrib : view.attributes) {
      descriptions.push_back(
          intl::VertexInputAttributeDescription{}
              .setLocation(CAST_TO_UINT(attrib.location))
              .setBinding(CAST_TO_UINT(view.binding_point))
              .setFormat(type::ConvertDataFormat(attrib.format))
              .setOffset(CAST_TO_UINT(attrib.offset)));
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
  return intl::PipelineInputAssemblyStateCreateInfo{}.setTopology(
      type::ConvertPrimitiveTopology(descriptor.primitive_topology));
}

intl::PipelineViewportStateCreateInfo GetViewportStateCreateInfo() {
  return intl::PipelineViewportStateCreateInfo{}
      .setViewportCount(1)
      .setScissorCount(1);
}

intl::PipelineRasterizationStateCreateInfo GetRasterizationStateCreateInfo(
    const GraphicsPipelineDescriptor& descriptor) {
  return intl::PipelineRasterizationStateCreateInfo{}
      .setCullMode(intl::CullModeFlagBits::eBack)
      .setFrontFace(descriptor.viewport_config.flip_y
                        ? intl::FrontFace::eCounterClockwise
                        : intl::FrontFace::eClockwise)
      .setLineWidth(1.0f);
}

intl::PipelineMultisampleStateCreateInfo GetMultisampleStateCreateInfo(
    intl::SampleCountFlagBits sample_count) {
  return intl::PipelineMultisampleStateCreateInfo{}.setRasterizationSamples(
      sample_count);
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
      .setBack(
          CreateStencilOpState(stencil_test.tests[StencilTest::kBackFaceIndex]))
      .setMinDepthBounds(0.0f)
      .setMaxDepthBounds(1.0f);
}

std::vector<intl::PipelineColorBlendAttachmentState>
CreateColorBlendAttachmentStates(const GraphicsPipelineDescriptor& descriptor,
                                 size_t color_attachment_count) {
  std::vector<intl::PipelineColorBlendAttachmentState> color_blend_states(
      color_attachment_count,
      intl::PipelineColorBlendAttachmentState{}.setColorWriteMask(
          intl::ColorComponentFlags{
              intl::FlagTraits<intl::ColorComponentFlagBits>::allFlags}));
  for (const auto& [location, optional_color_blend] :
       descriptor.color_attachment_map) {
    ASSERT_TRUE(
        location >= 0 && static_cast<size_t>(location) < color_attachment_count,
        std::format("Color attachment location {} out of range ({})", location,
                    color_attachment_count));
    if (!optional_color_blend.has_value()) {
      continue;
    }
    const auto& color_blend = optional_color_blend.value();
    color_blend_states[location] =
        intl::PipelineColorBlendAttachmentState{}
            .setSrcColorBlendFactor(
                type::ConvertBlendFactor(color_blend.src_color_blend_factor))
            .setDstColorBlendFactor(
                type::ConvertBlendFactor(color_blend.dst_color_blend_factor))
            .setColorBlendOp(type::ConvertBlendOp(color_blend.color_blend_op))
            .setSrcAlphaBlendFactor(
                type::ConvertBlendFactor(color_blend.src_alpha_blend_factor))
            .setDstColorBlendFactor(
                type::ConvertBlendFactor(color_blend.dst_alpha_blend_factor))
            .setAlphaBlendOp(type::ConvertBlendOp(color_blend.alpha_blend_op))
            .setColorWriteMask(intl::ColorComponentFlags{
                intl::FlagTraits<intl::ColorComponentFlagBits>::allFlags});
  }
  return color_blend_states;
}

intl::PipelineColorBlendStateCreateInfo GetColorBlendStateCreateInfo(
    const std::vector<intl::PipelineColorBlendAttachmentState>* states) {
  return intl::PipelineColorBlendStateCreateInfo{}.setAttachments(*states);
}

}  // namespace

PipelineReflection ReflectSpirv(std::span<const uint32_t> spirv_words,
                                intl::ShaderStageFlagBits* out_stage) {
  ASSERT_NON_EMPTY(spirv_words, "SPIR-V buffer is empty");
  spirv_cross::Compiler compiler{spirv_words.data(), spirv_words.size()};
  const intl::ShaderStageFlagBits stage =
      ConvertExecutionModel(compiler.get_execution_model());
  if (out_stage != nullptr) {
    *out_stage = stage;
  }

  PipelineReflection reflection;
  const spirv_cross::ShaderResources resources =
      compiler.get_shader_resources();
  AppendReflectedResources(compiler, resources.uniform_buffers,
                           intl::DescriptorType::eUniformBuffer, stage,
                           &reflection.bindings);
  AppendReflectedResources(compiler, resources.storage_buffers,
                           intl::DescriptorType::eStorageBuffer, stage,
                           &reflection.bindings);
  AppendReflectedResources(compiler, resources.sampled_images,
                           intl::DescriptorType::eCombinedImageSampler, stage,
                           &reflection.bindings);
  AppendReflectedResources(compiler, resources.separate_images,
                           intl::DescriptorType::eSampledImage, stage,
                           &reflection.bindings);
  AppendReflectedResources(compiler, resources.separate_samplers,
                           intl::DescriptorType::eSampler, stage,
                           &reflection.bindings);
  AppendReflectedResources(compiler, resources.storage_images,
                           intl::DescriptorType::eStorageImage, stage,
                           &reflection.bindings);
  AppendReflectedResources(compiler, resources.subpass_inputs,
                           intl::DescriptorType::eInputAttachment, stage,
                           &reflection.bindings);

  std::sort(reflection.bindings.begin(), reflection.bindings.end(),
            [](const ReflectedBinding& lhs, const ReflectedBinding& rhs) {
              return std::tie(lhs.set, lhs.binding) <
                     std::tie(rhs.set, rhs.binding);
            });

  for (const spirv_cross::Resource& pc : resources.push_constant_buffers) {
    const spirv_cross::SPIRType& block_type =
        compiler.get_type(pc.base_type_id);
    uint32_t offset = 0;
    if (!block_type.member_types.empty()) {
      offset = compiler.get_member_decoration(pc.base_type_id, 0,
                                              spv::DecorationOffset);
      for (uint32_t i = 1; i < block_type.member_types.size(); ++i) {
        offset =
            std::min(offset, compiler.get_member_decoration(
                                 pc.base_type_id, i, spv::DecorationOffset));
      }
    }
    const uint32_t end =
        static_cast<uint32_t>(compiler.get_declared_struct_size(block_type));
    if (end > offset) {
      reflection.push_constant_ranges.push_back(ReflectedPushConstantRange{
          .stage_flags = stage,
          .offset = offset,
          .size = end - offset,
      });
    }
  }
  return reflection;
}

PipelineReflection MergeReflections(
    std::span<const PipelineReflection> stage_reflections) {
  std::map<std::pair<uint32_t, uint32_t>, ReflectedBinding> binding_map;
  std::map<std::pair<uint32_t, uint32_t>, intl::ShaderStageFlags>
      push_constant_map;

  for (const auto& stage_reflection : stage_reflections) {
    for (const auto& binding : stage_reflection.bindings) {
      const auto key = std::make_pair(binding.set, binding.binding);
      const auto [iter, inserted] = binding_map.emplace(key, binding);
      if (!inserted) {
        ASSERT_TRUE(
            iter->second.descriptor_type == binding.descriptor_type &&
                iter->second.descriptor_count == binding.descriptor_count,
            std::format("Incompatible descriptor at set {}, binding {}",
                        binding.set, binding.binding));
        iter->second.stage_flags |= binding.stage_flags;
      }
    }

    for (const auto& range : stage_reflection.push_constant_ranges) {
      const auto key = std::make_pair(range.offset, range.size);
      push_constant_map[key] |= range.stage_flags;
    }
  }

  PipelineReflection merged;
  merged.bindings.reserve(binding_map.size());
  for (auto& [_, binding] : binding_map) {
    merged.bindings.push_back(std::move(binding));
  }
  merged.push_constant_ranges.reserve(push_constant_map.size());
  for (const auto& [offset_size, stage_flags] : push_constant_map) {
    merged.push_constant_ranges.push_back(ReflectedPushConstantRange{
        .stage_flags = stage_flags,
        .offset = offset_size.first,
        .size = offset_size.second,
    });
  }
  return merged;
}

ShaderModule::ShaderModule(const SharedContext& context,
                           std::string_view file_path)
    : WithSharedContext{context} {
  const common::Data file_data = common::file::LoadDataFromFile(file_path);
  ASSERT_TRUE(file_data.size() % sizeof(uint32_t) == 0,
              std::format("Invalid SPIR-V byte size in '{}'", file_path));
  const std::span<const uint32_t> spirv_words{
      file_data.data<uint32_t>(), file_data.size() / sizeof(uint32_t)};
  reflection_ = ReflectSpirv(spirv_words, &stage_);

  const auto shader_module_create_info =
      intl::ShaderModuleCreateInfo{}
          .setCodeSize(file_data.size())
          .setPCode(file_data.data<uint32_t>());
  shader_module_ = vk_device().createShaderModule(shader_module_create_info,
                                                  vk_host_allocator());
}

Pipeline::Pipeline(const SharedContext& context,
                   const GraphicsPipelineDescriptor& descriptor,
                   intl::SampleCountFlagBits sample_count,
                   const RenderingInfo& rendering_info)
    : WithSharedContext{context},
      name_{descriptor.pipeline_name},
      binding_point_{intl::PipelineBindPoint::eGraphics} {
  const auto shader_stages =
      CreateShaderStages(context_, descriptor.shader_path_map);
  std::vector<PipelineReflection> stage_reflections;
  stage_reflections.reserve(shader_stages.size());
  for (const auto& stage : shader_stages) {
    stage_reflections.push_back((*stage.module).reflection());
  }
  InitPipelineLayout(stage_reflections, descriptor.uniform_descriptor);

  const auto shader_stage_create_infos =
      GetShaderStageCreateInfos(&shader_stages);

  const auto vertex_input_binding_descs =
      CreateVertexInputBindingDescriptions(descriptor);
  const auto vertex_input_attribute_descs =
      CreateVertexInputAttributeDescriptions(descriptor);
  const auto vertex_input_state_create_info = GetVertexInputStateCreateInfo(
      &vertex_input_binding_descs, &vertex_input_attribute_descs);

  const auto viewport_state_create_info = GetViewportStateCreateInfo();

  const auto color_blend_attachment_states = CreateColorBlendAttachmentStates(
      descriptor, rendering_info.color_attachment_formats.size());
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

  constexpr std::array kDynamicStates{
      intl::DynamicState::eViewport,
      intl::DynamicState::eScissor,
  };
  const auto dynamic_state_create_info =
      intl::PipelineDynamicStateCreateInfo{}.setDynamicStates(kDynamicStates);

  const auto rendering_create_info =
      intl::PipelineRenderingCreateInfo{}
          .setColorAttachmentFormats(rendering_info.color_attachment_formats)
          .setDepthAttachmentFormat(rendering_info.depth_attachment_format)
          .setStencilAttachmentFormat(rendering_info.stencil_attachment_format);

  const auto pipeline_create_info =
      intl::GraphicsPipelineCreateInfo{}
          .setPNext(&rendering_create_info)
          .setStages(shader_stage_create_infos)
          .setPVertexInputState(&vertex_input_state_create_info)
          .setPInputAssemblyState(&input_assembly_state_create_info)
          .setPViewportState(&viewport_state_create_info)
          .setPRasterizationState(&rasterization_state_create_info)
          .setPMultisampleState(&multisample_state_create_info)
          .setPDepthStencilState(&depth_stencil_state_create_info)
          .setPColorBlendState(&color_blend_state_create_info)
          .setPDynamicState(&dynamic_state_create_info)
          .setLayout(pipeline_layout_);
  const auto result = vk_device().createGraphicsPipeline(
      intl::PipelineCache{}, pipeline_create_info, vk_host_allocator());
  ASSERT_SUCCESS(result.result, "Failed to create graphics pipeline");
  pipeline_ = result.value;
}

Pipeline::Pipeline(const SharedContext& context,
                   const ir::ComputePipelineDescriptor& descriptor)
    : WithSharedContext{context},
      name_{descriptor.pipeline_name},
      binding_point_{intl::PipelineBindPoint::eCompute} {
  const auto shader_stages = CreateShaderStages(
      context_, {{ir::shader_stage::COMPUTE, descriptor.shader_path}});
  const std::array stage_reflections{(*shader_stages[0].module).reflection()};
  InitPipelineLayout(stage_reflections, descriptor.uniform_descriptor);

  const auto shader_stage_create_infos =
      GetShaderStageCreateInfos(&shader_stages);

  const auto pipeline_create_info = intl::ComputePipelineCreateInfo{}
                                        .setStage(shader_stage_create_infos[0])
                                        .setLayout(pipeline_layout_);
  const auto result = vk_device().createComputePipeline(
      intl::PipelineCache{}, pipeline_create_info, vk_host_allocator());
  ASSERT_SUCCESS(result.result, "Failed to create compute pipeline");
  pipeline_ = result.value;
}

void Pipeline::InitPipelineLayout(
    std::span<const PipelineReflection> stage_reflections,
    const PipelineDescriptor::UniformDescriptor& uniform_descriptor) {
  reflection_ = MergeReflections(stage_reflections);
  if (reflection_.push_constant_ranges.empty() &&
      !uniform_descriptor.push_constant_ranges.empty()) {
    for (const auto& range : uniform_descriptor.push_constant_ranges) {
      reflection_.push_constant_ranges.push_back(ReflectedPushConstantRange{
          .stage_flags = type::ConvertShaderStages(range.shader_stages),
          .offset = CAST_TO_UINT(range.offset),
          .size = CAST_TO_UINT(range.size),
      });
    }
  }

  descriptor_set_layouts_ = CreateDescriptorSetLayouts(*context_, reflection_);
  const auto push_constant_ranges =
      CreatePushConstantRanges(reflection_.push_constant_ranges);

  const auto layout_create_info = GetPipelineLayoutCreateInfo(
      &descriptor_set_layouts_, &push_constant_ranges);
  pipeline_layout_ =
      vk_device().createPipelineLayout(layout_create_info, vk_host_allocator());
}

void Pipeline::Bind(intl::CommandBuffer command_buffer) const {
  command_buffer.bindPipeline(binding_point_, pipeline_);
}

void Pipeline::PushDescriptors(intl::CommandBuffer command_buffer,
                               std::span<const BufferBinding> buffer_bindings,
                               std::span<const ImageBinding> image_bindings,
                               uint32_t set) const {
  if (buffer_bindings.empty() && image_bindings.empty()) {
    return;
  }
  ASSERT_TRUE(set == 0 && set < descriptor_set_layouts_.size(),
              std::format("Push descriptor set {} is out of range ({})", set,
                          descriptor_set_layouts_.size()));

  std::vector<intl::DescriptorBufferInfo> buffer_infos;
  std::vector<intl::DescriptorImageInfo> image_infos;
  std::vector<intl::WriteDescriptorSet> writes;
  buffer_infos.reserve(buffer_bindings.size());
  image_infos.reserve(image_bindings.size());
  writes.reserve(buffer_bindings.size() + image_bindings.size());

  for (const auto& entry : buffer_bindings) {
    ASSERT_NON_NULL(entry.buffer, "Buffer binding must not be nullptr");
    const ReflectedBinding* reflected =
        FindReflectedBinding(reflection_, set, entry.binding);
    ASSERT_NON_NULL(
        reflected,
        std::format("No descriptor at set {}, binding {}", set, entry.binding));
    ASSERT_TRUE(IsBufferDescriptorType(reflected->descriptor_type),
                std::format("Descriptor at set {}, binding {} is not a buffer",
                            set, entry.binding));
    ASSERT_TRUE(entry.array_element < reflected->descriptor_count,
                std::format("Array element {} exceeds descriptor count {}",
                            entry.array_element, reflected->descriptor_count));

    buffer_infos.push_back(intl::DescriptorBufferInfo{}
                               .setBuffer(entry.buffer->vk_buffer())
                               .setOffset(entry.offset)
                               .setRange(entry.range));
    writes.push_back(intl::WriteDescriptorSet{}
                         .setDstBinding(entry.binding)
                         .setDstArrayElement(entry.array_element)
                         .setDescriptorCount(1)
                         .setDescriptorType(reflected->descriptor_type)
                         .setPBufferInfo(&buffer_infos.back()));
  }

  for (const auto& entry : image_bindings) {
    ASSERT_NON_NULL(entry.image, "Image binding must not be nullptr");
    const ReflectedBinding* reflected =
        FindReflectedBinding(reflection_, set, entry.binding);
    ASSERT_NON_NULL(
        reflected,
        std::format("No descriptor at set {}, binding {}", set, entry.binding));
    ASSERT_TRUE(IsImageDescriptorType(reflected->descriptor_type),
                std::format("Descriptor at set {}, binding {} is not an image",
                            set, entry.binding));
    ASSERT_TRUE(entry.array_element < reflected->descriptor_count,
                std::format("Array element {} exceeds descriptor count {}",
                            entry.array_element, reflected->descriptor_count));

    if (reflected->descriptor_type ==
        intl::DescriptorType::eCombinedImageSampler) {
      ASSERT_NON_NULL(
          entry.sampler,
          std::format("Combined image sampler at set {}, binding {} requires a "
                      "non-null sampler",
                      set, entry.binding));
    }
    image_infos.push_back(
        intl::DescriptorImageInfo{}
            .setSampler(entry.sampler != nullptr ? **entry.sampler
                                                 : intl::Sampler{})
            .setImageView(entry.image->image_view(entry.image_index))
            .setImageLayout(entry.layout));
    writes.push_back(intl::WriteDescriptorSet{}
                         .setDstBinding(entry.binding)
                         .setDstArrayElement(entry.array_element)
                         .setDescriptorCount(1)
                         .setDescriptorType(reflected->descriptor_type)
                         .setPImageInfo(&image_infos.back()));
  }

  command_buffer.pushDescriptorSetKHR(binding_point_, pipeline_layout_, set,
                                      writes);
}

void Pipeline::PushConstants(intl::CommandBuffer command_buffer,
                             intl::ShaderStageFlags stage_flags,
                             uint32_t offset, uint32_t size,
                             const void* data) const {
  ASSERT_NON_NULL(data, "Push constant data must not be nullptr");
  ASSERT_TRUE(size > 0, "Push constant size must be positive");
  const bool matches_reflected =
      std::any_of(reflection_.push_constant_ranges.begin(),
                  reflection_.push_constant_ranges.end(),
                  [&](const ReflectedPushConstantRange& range) {
                    return static_cast<bool>(range.stage_flags & stage_flags) &&
                           offset >= range.offset &&
                           offset + size <= range.offset + range.size;
                  });
  ASSERT_TRUE(matches_reflected,
              std::format("Push constant range [{}, {}) does not match any "
                          "reflected push constant block",
                          offset, offset + size));
  command_buffer.pushConstants(pipeline_layout_, stage_flags, offset, size,
                               data);
}

Pipeline::~Pipeline() {
  context_->DeviceDestroy(pipeline_);
  context_->DeviceDestroy(pipeline_layout_);
  for (intl::DescriptorSetLayout layout : descriptor_set_layouts_) {
    context_->DeviceDestroy(layout);
  }
#ifndef NDEBUG
  LOG_INFO << std::format("Pipeline '{}' destructed", name_);
#endif  // DEBUG
}

}  // namespace lighter::renderer::vk
