//
//  pipeline_cache_test.cc
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/pipeline_cache.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace lighter::renderer::vk {
namespace {

TEST(PipelineCacheKeyTest, GraphicsPipelineKeyDistinguishesState) {
  const auto base_desc =
      ir::GraphicsPipelineDescriptor{}
          .SetName("Tri")
          .SetShader(ir::shader_stage::VERTEX, "tri.vert.spv")
          .SetShader(ir::shader_stage::FRAGMENT, "tri.frag.spv")
          .UseColorAttachment(0, std::nullopt);

  const Pipeline::RenderingInfo info_bgra{
      .color_attachment_formats = {intl::Format::eB8G8R8A8Unorm},
  };
  const Pipeline::RenderingInfo info_rgba{
      .color_attachment_formats = {intl::Format::eR8G8B8A8Unorm},
  };

  const std::string key1 = MakeGraphicsPipelineCacheKey(
      base_desc, intl::SampleCountFlagBits::e1, info_bgra);
  const std::string key2 = MakeGraphicsPipelineCacheKey(
      base_desc, intl::SampleCountFlagBits::e1, info_bgra);
  const std::string key3 = MakeGraphicsPipelineCacheKey(
      base_desc, intl::SampleCountFlagBits::e4, info_bgra);
  const std::string key4 = MakeGraphicsPipelineCacheKey(
      base_desc, intl::SampleCountFlagBits::e1, info_rgba);

  EXPECT_EQ(key1, key2);
  EXPECT_NE(key1, key3);
  EXPECT_NE(key1, key4);
}

TEST(PipelineCacheKeyTest, ComputeAndSamplerKeysDistinguishInputs) {
  const auto compute_a =
      ir::ComputePipelineDescriptor{}.SetShader("a.comp.spv");
  const auto compute_b =
      ir::ComputePipelineDescriptor{}.SetShader("b.comp.spv");
  EXPECT_EQ(MakeComputePipelineCacheKey(compute_a),
            MakeComputePipelineCacheKey(compute_a));
  EXPECT_NE(MakeComputePipelineCacheKey(compute_a),
            MakeComputePipelineCacheKey(compute_b));

  const ir::SamplerDescriptor linear_clamp{
      .filter_type = ir::FilterType::kLinear,
      .address_mode = ir::SamplerAddressMode::kClampToEdge,
  };
  const ir::SamplerDescriptor nearest_clamp{
      .filter_type = ir::FilterType::kNearest,
      .address_mode = ir::SamplerAddressMode::kClampToEdge,
  };
  EXPECT_EQ(MakeSamplerCacheKey(linear_clamp, /*mip_levels=*/1),
            MakeSamplerCacheKey(linear_clamp, /*mip_levels=*/1));
  EXPECT_NE(MakeSamplerCacheKey(linear_clamp, /*mip_levels=*/1),
            MakeSamplerCacheKey(nearest_clamp, /*mip_levels=*/1));
  EXPECT_NE(MakeSamplerCacheKey(linear_clamp, /*mip_levels=*/1),
            MakeSamplerCacheKey(linear_clamp, /*mip_levels=*/4));
}

}  // namespace
}  // namespace lighter::renderer::vk
