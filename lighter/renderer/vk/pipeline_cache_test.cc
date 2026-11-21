#include "lighter/renderer/vk/pipeline_cache.h"

#include <gtest/gtest.h>

#include <cstring>
#include <new>
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

TEST(PipelineCacheKeyTest, DefaultConstructedDescriptorHasDeterministicState) {
  alignas(ir::GraphicsPipelineDescriptor) unsigned char
      dirty_storage[sizeof(ir::GraphicsPipelineDescriptor)];
  std::memset(dirty_storage, 0xA5, sizeof(dirty_storage));
  auto* dirty_default = new (dirty_storage) ir::GraphicsPipelineDescriptor;

  EXPECT_FALSE(dirty_default->depth_test.has_value());
  EXPECT_FALSE(dirty_default->stencil_test.has_value());

  const ir::GraphicsPipelineDescriptor value_init{};
  const Pipeline::RenderingInfo info{
      .color_attachment_formats = {intl::Format::eB8G8R8A8Unorm},
  };
  EXPECT_EQ(MakeGraphicsPipelineCacheKey(*dirty_default,
                                         intl::SampleCountFlagBits::e1, info),
            MakeGraphicsPipelineCacheKey(value_init,
                                         intl::SampleCountFlagBits::e1, info));
  dirty_default->~GraphicsPipelineDescriptor();

  auto with_depth = value_init;
  with_depth.EnableDepthTestOnly();
  auto with_stencil = value_init;
  with_stencil.EnableStencilTest({}, {});
  EXPECT_NE(MakeGraphicsPipelineCacheKey(value_init,
                                         intl::SampleCountFlagBits::e1, info),
            MakeGraphicsPipelineCacheKey(with_depth,
                                         intl::SampleCountFlagBits::e1, info));
  EXPECT_NE(MakeGraphicsPipelineCacheKey(value_init,
                                         intl::SampleCountFlagBits::e1, info),
            MakeGraphicsPipelineCacheKey(with_stencil,
                                         intl::SampleCountFlagBits::e1, info));
}

}  // namespace
}  // namespace lighter::renderer::vk
