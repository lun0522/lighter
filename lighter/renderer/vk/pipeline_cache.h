//
//  pipeline_cache.h
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_PIPELINE_CACHE_H
#define LIGHTER_RENDERER_VK_PIPELINE_CACHE_H

#include <memory>
#include <string>
#include <unordered_map>

#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/image.h"
#include "lighter/renderer/vk/pipeline.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk {

// Serializes descriptors into deterministic cache lookup keys.
std::string MakeGraphicsPipelineCacheKey(
    const ir::GraphicsPipelineDescriptor& descriptor,
    intl::SampleCountFlagBits sample_count,
    const Pipeline::RenderingInfo& rendering_info);

std::string MakeComputePipelineCacheKey(
    const ir::ComputePipelineDescriptor& descriptor);

std::string MakeSamplerCacheKey(const ir::SamplerDescriptor& descriptor,
                                int mip_levels);

// Lazily creates and caches graphics/compute Pipeline and Sampler objects by
// their serialized descriptor keys for reuse across RenderGraph frames.
class PipelineCache : public WithSharedContext {
 public:
  explicit PipelineCache(const SharedContext& context)
      : WithSharedContext{context} {}

  // This class is neither copyable nor movable.
  PipelineCache(const PipelineCache&) = delete;
  PipelineCache& operator=(const PipelineCache&) = delete;

  const Pipeline& GetOrCreateGraphicsPipeline(
      const ir::GraphicsPipelineDescriptor& descriptor,
      intl::SampleCountFlagBits sample_count,
      const Pipeline::RenderingInfo& rendering_info);

  const Pipeline& GetOrCreateComputePipeline(
      const ir::ComputePipelineDescriptor& descriptor);

  // Returns nullptr when 'descriptor' is nullptr.
  const Sampler* GetOrCreateSampler(const ir::SamplerDescriptor* descriptor,
                                    int mip_levels);

 private:
  std::unordered_map<std::string, std::unique_ptr<Pipeline>>
      graphics_pipelines_;
  std::unordered_map<std::string, std::unique_ptr<Pipeline>> compute_pipelines_;
  std::unordered_map<std::string, std::unique_ptr<Sampler>> samplers_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_PIPELINE_CACHE_H
