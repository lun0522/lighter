//
//  pipeline_cache.cc
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/pipeline_cache.h"

#include <algorithm>
#include <format>
#include <sstream>
#include <utility>
#include <vector>

namespace lighter::renderer::vk {
namespace {

void SerializeUniformDescriptor(
    const ir::PipelineDescriptor::UniformDescriptor& uniform,
    std::ostringstream* out) {
  *out << uniform.push_constant_ranges.size() << ';';
  for (const auto& range : uniform.push_constant_ranges) {
    *out << range.shader_stages << ',' << range.offset << ',' << range.size
         << ';';
  }
}

void SerializeString(std::string_view value, std::ostringstream* out) {
  *out << value.size() << ':' << value << ';';
}

}  // namespace

std::string MakeGraphicsPipelineCacheKey(
    const ir::GraphicsPipelineDescriptor& descriptor,
    intl::SampleCountFlagBits sample_count,
    const Pipeline::RenderingInfo& rendering_info) {
  std::ostringstream out;
  out << "G;" << static_cast<int>(sample_count) << ';';
  out << rendering_info.color_attachment_formats.size() << ';';
  for (intl::Format fmt : rendering_info.color_attachment_formats) {
    out << static_cast<int>(fmt) << ',';
  }
  out << static_cast<int>(rendering_info.depth_attachment_format) << ','
      << static_cast<int>(rendering_info.stencil_attachment_format) << ';';

  std::vector<std::pair<int, std::string_view>> shaders;
  shaders.reserve(descriptor.shader_path_map.size());
  for (const auto& [stage, path] : descriptor.shader_path_map) {
    shaders.emplace_back(static_cast<int>(stage), path);
  }
  std::sort(shaders.begin(), shaders.end());
  out << shaders.size() << ';';
  for (const auto& [stage, path] : shaders) {
    out << stage << ':';
    SerializeString(path, &out);
  }

  std::vector<int> blend_locs;
  blend_locs.reserve(descriptor.color_attachment_map.size());
  for (const auto& [loc, _] : descriptor.color_attachment_map) {
    blend_locs.push_back(loc);
  }
  std::sort(blend_locs.begin(), blend_locs.end());
  out << blend_locs.size() << ';';
  for (int loc : blend_locs) {
    out << loc << ':';
    const auto& blend = descriptor.color_attachment_map.at(loc);
    if (blend.has_value()) {
      out << static_cast<int>(blend->src_color_blend_factor) << ','
          << static_cast<int>(blend->dst_color_blend_factor) << ','
          << static_cast<int>(blend->color_blend_op) << ','
          << static_cast<int>(blend->src_alpha_blend_factor) << ','
          << static_cast<int>(blend->dst_alpha_blend_factor) << ','
          << static_cast<int>(blend->alpha_blend_op);
    } else {
      out << "none";
    }
    out << ';';
  }

  out << descriptor.vertex_buffer_views.size() << ';';
  for (const auto& view : descriptor.vertex_buffer_views) {
    out << static_cast<int>(view.input_rate) << ',' << view.binding_point << ','
        << view.stride << ',' << view.attributes.size() << ':';
    for (const auto& attr : view.attributes) {
      out << attr.location << ',' << static_cast<int>(attr.format) << ','
          << attr.offset << '/';
    }
    out << ';';
  }

  out << descriptor.depth_test.enable_test << ','
      << descriptor.depth_test.enable_write << ','
      << static_cast<int>(descriptor.depth_test.compare_op) << ';';
  out << descriptor.stencil_test.enable_test << ';';
  for (const auto& face : descriptor.stencil_test.tests) {
    out << static_cast<int>(face.stencil_fail_op) << ','
        << static_cast<int>(face.stencil_and_depth_pass_op) << ','
        << static_cast<int>(face.stencil_pass_depth_fail_op) << ','
        << static_cast<int>(face.compare_op) << ',' << face.compare_mask << ','
        << face.write_mask << ',' << face.reference << ';';
  }
  out << descriptor.viewport_config.flip_y << ','
      << static_cast<int>(descriptor.primitive_topology) << ';';
  SerializeUniformDescriptor(descriptor.uniform_descriptor, &out);
  return out.str();
}

std::string MakeComputePipelineCacheKey(
    const ir::ComputePipelineDescriptor& descriptor) {
  std::ostringstream out;
  out << "C;";
  SerializeString(descriptor.shader_path, &out);
  SerializeUniformDescriptor(descriptor.uniform_descriptor, &out);
  return out.str();
}

std::string MakeSamplerCacheKey(const ir::SamplerDescriptor& descriptor,
                                int mip_levels) {
  return std::format("{};{};{}", static_cast<int>(descriptor.filter_type),
                     static_cast<int>(descriptor.address_mode), mip_levels);
}

const Pipeline& PipelineCache::GetOrCreateGraphicsPipeline(
    const ir::GraphicsPipelineDescriptor& descriptor,
    intl::SampleCountFlagBits sample_count,
    const Pipeline::RenderingInfo& rendering_info) {
  const std::string key =
      MakeGraphicsPipelineCacheKey(descriptor, sample_count, rendering_info);
  auto [iter, inserted] = graphics_pipelines_.try_emplace(key, nullptr);
  if (inserted) {
    iter->second = std::make_unique<Pipeline>(context_, descriptor,
                                              sample_count, rendering_info);
  }
  return *iter->second;
}

const Pipeline& PipelineCache::GetOrCreateComputePipeline(
    const ir::ComputePipelineDescriptor& descriptor) {
  const std::string key = MakeComputePipelineCacheKey(descriptor);
  auto [iter, inserted] = compute_pipelines_.try_emplace(key, nullptr);
  if (inserted) {
    iter->second = std::make_unique<Pipeline>(context_, descriptor);
  }
  return *iter->second;
}

const Sampler* PipelineCache::GetOrCreateSampler(
    const ir::SamplerDescriptor* descriptor, int mip_levels) {
  if (descriptor == nullptr) {
    return nullptr;
  }
  const std::string key = MakeSamplerCacheKey(*descriptor, mip_levels);
  auto [iter, inserted] = samplers_.try_emplace(key, nullptr);
  if (inserted) {
    iter->second = std::make_unique<Sampler>(context_, *descriptor, mip_levels);
  }
  return iter->second.get();
}

}  // namespace lighter::renderer::vk
