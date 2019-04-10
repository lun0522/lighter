//
//  descriptor.cc
//
//  Created by Pujun Lun on 3/17/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/descriptor.h"

#include <stdexcept>

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/context.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using descriptor::ResourceInfo;
using std::vector;

VkDescriptorPool CreateDescriptorPool(
 const SharedContext& context,
    const vector<ResourceInfo>& resource_infos) {
  vector<VkDescriptorPoolSize> pool_sizes;
  pool_sizes.reserve(resource_infos.size());
  for (const auto& info : resource_infos) {
    pool_sizes.emplace_back(VkDescriptorPoolSize{
        info.descriptor_type,
        CONTAINER_SIZE(info.binding_points),
    });
  }

  VkDescriptorPoolCreateInfo pool_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      /*pNext=*/nullptr,
      common::util::nullflag,
      /*maxSets=*/1,
      CONTAINER_SIZE(pool_sizes),
      pool_sizes.data(),
  };

  VkDescriptorPool pool;
  ASSERT_SUCCESS(vkCreateDescriptorPool(*context->device(), &pool_info,
                                        context->allocator(), &pool),
                 "Failed to create descriptor pool");
  return pool;
}

VkDescriptorSetLayout CreateDescriptorSetLayout(
 const SharedContext& context,
    const vector<ResourceInfo>& resource_infos) {
  size_t total_bindings = 0;
  for (const auto& info : resource_infos) {
    total_bindings += info.binding_points.size();
  }

  vector<VkDescriptorSetLayoutBinding> layout_bindings;
  layout_bindings.reserve(total_bindings);
  for (const auto& info : resource_infos) {
    for (auto binding : info.binding_points) {
      layout_bindings.emplace_back(VkDescriptorSetLayoutBinding{
          binding,
          info.descriptor_type,
          /*descriptorCount=*/1,  // will be different for uniform array
          info.shader_stage,
          /*pImmutableSamplers=*/nullptr,
      });
    }
  }

  VkDescriptorSetLayoutCreateInfo layout_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      /*pNext=*/nullptr,
      common::util::nullflag,
      /*bindingCount=*/CONTAINER_SIZE(layout_bindings),
      layout_bindings.data(),
  };

  VkDescriptorSetLayout layout;
  ASSERT_SUCCESS(vkCreateDescriptorSetLayout(*context->device(), &layout_info,
                                             context->allocator(), &layout),
                 "Failed to create descriptor set layout");
  return layout;
}

VkDescriptorSet CreateDescriptorSet(const SharedContext& context,
                                    const VkDescriptorPool& pool,
                                    const VkDescriptorSetLayout& layout) {
  VkDescriptorSetAllocateInfo alloc_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      pool,
      /*descriptorSetCount=*/1,
      &layout,
  };

  VkDescriptorSet set;
  ASSERT_SUCCESS(
      vkAllocateDescriptorSets(*context->device(), &alloc_info, &set),
      "Failed to allocate descriptor set");
  return set;
}

} /* namespace */

void Descriptor::Init(SharedContext context,
                      const vector<ResourceInfo>& resource_infos) {
  context_ = std::move(context);
  pool_ = CreateDescriptorPool(context_, resource_infos);
  layout_ = CreateDescriptorSetLayout(context_, resource_infos);
  set_ = CreateDescriptorSet(context_, pool_, layout_);
}

void Descriptor::UpdateBufferInfos(
    const ResourceInfo& resource_info,
    const vector<VkDescriptorBufferInfo>& buffer_infos) {
  if (resource_info.binding_points.size() != buffer_infos.size()) {
    throw std::runtime_error{"Failed to update image infos"};
  }

  vector<VkWriteDescriptorSet> write_desc_sets(buffer_infos.size());
  for (size_t i = 0; i < buffer_infos.size(); ++i) {
    write_desc_sets[i] = VkWriteDescriptorSet{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        /*pNext=*/nullptr,
        set_,
        resource_info.binding_points[i],
        /*dstArrayElement=*/0,  // target first descriptor in set
        /*descriptorCount=*/1,  // possible to update multiple descriptors
        resource_info.descriptor_type,
        /*pImageInfo=*/nullptr,
        &buffer_infos[i],
        /*pTexelBufferView=*/nullptr,
    };
  }
  vkUpdateDescriptorSets(*context_->device(), CONTAINER_SIZE(write_desc_sets),
                         write_desc_sets.data(), 0, nullptr);
}

void Descriptor::UpdateImageInfos(
    const ResourceInfo& resource_info,
    const vector<VkDescriptorImageInfo>& image_infos) {
  if (resource_info.binding_points.size() != image_infos.size()) {
    throw std::runtime_error{"Failed to update image infos"};
  }

  vector<VkWriteDescriptorSet> write_desc_sets(image_infos.size());
  for (size_t i = 0; i < image_infos.size(); ++i) {
    write_desc_sets[i] = VkWriteDescriptorSet{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        /*pNext=*/nullptr,
        set_,
        resource_info.binding_points[i],
        /*dstArrayElement=*/0,  // target first descriptor in set
        /*descriptorCount=*/1,  // possible to update multiple descriptors
        resource_info.descriptor_type,
        &image_infos[i],
        /*pBufferInfo=*/nullptr,
        /*pTexelBufferView=*/nullptr,
    };
  }
  vkUpdateDescriptorSets(*context_->device(), CONTAINER_SIZE(write_desc_sets),
                         write_desc_sets.data(), 0, nullptr);
}

Descriptor::~Descriptor() {
  vkDestroyDescriptorPool(*context_->device(), pool_, context_->allocator());
  // descriptor set is implicitly cleaned up with descriptor pool
  vkDestroyDescriptorSetLayout(*context_->device(), layout_,
                               context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
