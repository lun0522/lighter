//
//  pipeline_test.cc
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/pipeline.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <span>

#include "lighter/base/data.h"
#include "lighter/base/file.h"

namespace lighter::renderer::vk {
namespace {

PipelineReflection ReflectShaderFile(std::string_view relative_path) {
  const std::string spv_path = base::file::GetShaderBinaryPath(relative_path);
  const base::Data data = base::file::LoadDataFromFile(spv_path);
  EXPECT_EQ(data.size() % sizeof(uint32_t), 0u);
  return ReflectSpirv(std::span<const uint32_t>{
      data.data<uint32_t>(), data.size() / sizeof(uint32_t)});
}

TEST(PipelineReflectionTest, ReflectTriangleShaders) {
  const PipelineReflection vert = ReflectShaderFile("triangle/triangle.vert");
  EXPECT_TRUE(vert.bindings.empty());
  EXPECT_TRUE(vert.push_constant_ranges.empty());

  const PipelineReflection frag = ReflectShaderFile("triangle/triangle.frag");
  EXPECT_TRUE(frag.bindings.empty());
  ASSERT_EQ(frag.push_constant_ranges.size(), 1u);
  EXPECT_EQ(frag.push_constant_ranges[0].stage_flags,
            intl::ShaderStageFlagBits::eFragment);
  EXPECT_EQ(frag.push_constant_ranges[0].offset, 0u);
  EXPECT_EQ(frag.push_constant_ranges[0].size, sizeof(float));

  const std::array stages{vert, frag};
  const PipelineReflection merged = MergeReflections(stages);
  EXPECT_TRUE(merged.bindings.empty());
  ASSERT_EQ(merged.push_constant_ranges.size(), 1u);
  EXPECT_EQ(merged.push_constant_ranges[0].stage_flags,
            intl::ShaderStageFlagBits::eFragment);
}

TEST(PipelineReflectionTest, ReflectNanosuitUniformsAndSamplers) {
  const PipelineReflection vert = ReflectShaderFile("nanosuit/nanosuit.vert");
  ASSERT_EQ(vert.bindings.size(), 1u);
  EXPECT_EQ(vert.bindings[0].set, 0u);
  EXPECT_EQ(vert.bindings[0].binding, 0u);
  EXPECT_EQ(vert.bindings[0].descriptor_type,
            intl::DescriptorType::eUniformBuffer);
  EXPECT_EQ(vert.bindings[0].descriptor_count, 1u);
  EXPECT_EQ(vert.bindings[0].stage_flags, intl::ShaderStageFlagBits::eVertex);

  const PipelineReflection frag = ReflectShaderFile("nanosuit/nanosuit.frag");
  ASSERT_EQ(frag.bindings.size(), 4u);
  for (uint32_t i = 0; i < 4; ++i) {
    EXPECT_EQ(frag.bindings[i].set, 0u);
    EXPECT_EQ(frag.bindings[i].binding, i + 1);
    EXPECT_EQ(frag.bindings[i].descriptor_type,
              intl::DescriptorType::eCombinedImageSampler);
    EXPECT_EQ(frag.bindings[i].descriptor_count, 1u);
    EXPECT_EQ(frag.bindings[i].stage_flags,
              intl::ShaderStageFlagBits::eFragment);
  }
  ASSERT_EQ(frag.push_constant_ranges.size(), 1u);
  EXPECT_EQ(frag.push_constant_ranges[0].stage_flags,
            intl::ShaderStageFlagBits::eFragment);

  const std::array stages{vert, frag};
  const PipelineReflection merged = MergeReflections(stages);
  ASSERT_EQ(merged.bindings.size(), 5u);
  EXPECT_EQ(merged.bindings[0].binding, 0u);
  EXPECT_EQ(merged.bindings[4].binding, 4u);
}

TEST(PipelineReflectionTest, ReflectDescriptorArray) {
  const PipelineReflection frag = ReflectShaderFile("aurora/earth.frag");
  ASSERT_EQ(frag.bindings.size(), 1u);
  EXPECT_EQ(frag.bindings[0].set, 0u);
  EXPECT_EQ(frag.bindings[0].binding, 2u);
  EXPECT_EQ(frag.bindings[0].descriptor_type,
            intl::DescriptorType::eCombinedImageSampler);
  EXPECT_EQ(frag.bindings[0].descriptor_count, 2u);
  EXPECT_EQ(frag.bindings[0].stage_flags, intl::ShaderStageFlagBits::eFragment);
  ASSERT_EQ(frag.push_constant_ranges.size(), 1u);
}

TEST(PipelineReflectionTest, ReflectComputeStorageImages) {
  const PipelineReflection comp =
      ReflectShaderFile("shared/distance_field/jump_flooding.comp");
  ASSERT_EQ(comp.bindings.size(), 2u);
  EXPECT_EQ(comp.bindings[0].binding, 0u);
  EXPECT_EQ(comp.bindings[0].descriptor_type,
            intl::DescriptorType::eStorageImage);
  EXPECT_EQ(comp.bindings[0].stage_flags, intl::ShaderStageFlagBits::eCompute);
  EXPECT_EQ(comp.bindings[1].binding, 1u);
  EXPECT_EQ(comp.bindings[1].descriptor_type,
            intl::DescriptorType::eStorageImage);
  EXPECT_EQ(comp.bindings[1].stage_flags, intl::ShaderStageFlagBits::eCompute);
  ASSERT_EQ(comp.push_constant_ranges.size(), 1u);
  EXPECT_EQ(comp.push_constant_ranges[0].stage_flags,
            intl::ShaderStageFlagBits::eCompute);
  EXPECT_EQ(comp.push_constant_ranges[0].size, sizeof(int));
}

}  // namespace
}  // namespace lighter::renderer::vk
