//
//  descriptor.cc
//
//  Created by Pujun Lun on 3/17/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "descriptor.h"

#include "context.h"
#include "util.h"

namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

VkDescriptorPool CreateDescriptorPool(SharedContext context,
                                      VkDescriptorType type,
                                      uint32_t count) {
  VkDescriptorPoolSize pool_size{type, count};

  VkDescriptorPoolCreateInfo pool_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      /*maxSets=*/count,
      /*poolSizeCount=*/1,
      &pool_size,
  };

  VkDescriptorPool pool;
  ASSERT_SUCCESS(vkCreateDescriptorPool(*context->device(), &pool_info,
                                        context->allocator(), &pool),
                 "Failed to create descriptor pool");
  return pool;
}

vector<VkDescriptorSetLayout> CreateDescriptorSetLayouts(
    SharedContext context,
    VkDescriptorType descriptor_type,
    const vector<uint32_t>& binding_points,
    VkShaderStageFlags shader_stage) {
  // it is possible to use layout(set = 0, binding = 0) to bind multiple
  // descriptor sets to one binding point, which can be useful if we render
  // different objects with different buffers and descriptors, but use the same
  // uniform values. here we don't use this, so |descriptorCount| is simply 1
  VkDescriptorSetLayoutBinding layout_binding{
      0,  // will be set to actual binding points
      descriptor_type,
      /*descriptorCount=*/1,
      shader_stage,
      /*pImmutableSamplers=*/nullptr,
  };

  VkDescriptorSetLayoutCreateInfo layout_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      /*bindingCount=*/1,
      &layout_binding,
  };

  vector<VkDescriptorSetLayout> layouts(binding_points.size());
  for (size_t i = 0; i < binding_points.size(); ++i) {
    layout_binding.binding = binding_points[i];
    ASSERT_SUCCESS(
        vkCreateDescriptorSetLayout(*context->device(), &layout_info,
                                    context->allocator(), &layouts[i]),
        "Failed to create descriptor set layout");
  }
  return layouts;
}

vector<VkDescriptorSet> CreateDescriptorSets(
    SharedContext context,
    const VkDescriptorPool pool,
    const vector<VkDescriptorSetLayout>& layouts) {
  VkDescriptorSetAllocateInfo alloc_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      pool,
      CONTAINER_SIZE(layouts),
      layouts.data(),
  };

  vector<VkDescriptorSet> sets(layouts.size());
  ASSERT_SUCCESS(
      vkAllocateDescriptorSets(*context->device(), &alloc_info, sets.data()),
      "Failed to allocate descriptor sets");
  return sets;
}

} /* namespace */

void Descriptor::Init(SharedContext context,
                      VkDescriptorType descriptor_type,
                      const vector<uint32_t>& binding_points,
                      VkShaderStageFlags shader_stage) {
  context_ = context;
  descriptor_type_ = descriptor_type;
  binding_points_ = binding_points;

  descriptor_pool_ = CreateDescriptorPool(
      context_, descriptor_type_, CONTAINER_SIZE(binding_points_));
  descriptor_set_layouts_ = CreateDescriptorSetLayouts(
      context_, descriptor_type_, binding_points_, shader_stage);
  descriptor_sets_ = CreateDescriptorSets(
      context_, descriptor_pool_, descriptor_set_layouts_);
}

void Descriptor::UpdateBufferInfos(
    const vector<VkDescriptorBufferInfo>& buffer_infos) {
  if (buffer_infos.size() != descriptor_sets_.size())
    throw std::runtime_error{"Failed to update buffer infos"};

  vector<VkWriteDescriptorSet> write_desc_sets(buffer_infos.size());
  for (size_t i = 0; i < buffer_infos.size(); ++i) {
    write_desc_sets[i] = VkWriteDescriptorSet{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        /*pNext=*/nullptr,
        descriptor_sets_[i],
        binding_points_[i],
        /*dstArrayElement=*/0,  // target first descriptor in set
        /*descriptorCount=*/1,  // possible to update multiple descriptors
        descriptor_type_,
        /*pImageInfo=*/nullptr,
        &buffer_infos[i],
        /*pTexelBufferView=*/nullptr,
    };
  }
  vkUpdateDescriptorSets(*context_->device(), CONTAINER_SIZE(write_desc_sets),
                         write_desc_sets.data(), 0, nullptr);
}

void Descriptor::UpdateImageInfos(
    const vector<VkDescriptorImageInfo>& image_infos) {
  if (image_infos.size() != descriptor_sets_.size())
    throw std::runtime_error{"Failed to update image infos"};

  vector<VkWriteDescriptorSet> write_desc_sets(image_infos.size());
  for (size_t i = 0; i < image_infos.size(); ++i) {
    write_desc_sets[i] = VkWriteDescriptorSet{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        /*pNext=*/nullptr,
        descriptor_sets_[i],
        binding_points_[i],
        /*dstArrayElement=*/0,  // target first descriptor in set
        /*descriptorCount=*/1,  // possible to update multiple descriptors
        descriptor_type_,
        &image_infos[i],
        /*pBufferInfo=*/nullptr,
        /*pTexelBufferView=*/nullptr,
    };
  }
  vkUpdateDescriptorSets(*context_->device(), CONTAINER_SIZE(write_desc_sets),
                         write_desc_sets.data(), 0, nullptr);
}

Descriptor::~Descriptor() {
  vkDestroyDescriptorPool(*context_->device(), descriptor_pool_,
                          context_->allocator());
  // descriptor sets are implicitly cleaned up with descriptor pool
  for (auto& layout : descriptor_set_layouts_) {
    vkDestroyDescriptorSetLayout(*context_->device(), layout,
                                 context_->allocator());
  }
}

} /* namespace vulkan */
} /* namespace wrapper */
