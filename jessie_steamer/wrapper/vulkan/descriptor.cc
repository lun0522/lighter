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

using std::vector;

VkDescriptorPool CreateDescriptorPool(
    const SharedContext& context,
    const vector<Descriptor::Info>& descriptor_infos) {
  vector<VkDescriptorPoolSize> pool_sizes;
  pool_sizes.reserve(descriptor_infos.size());
  for (const auto& info : descriptor_infos) {
    uint32_t total_length = 0;
    for (const auto& binding : info.bindings) {
      total_length += binding.array_length;
    }
    pool_sizes.emplace_back(VkDescriptorPoolSize{
        info.descriptor_type,
        total_length,
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
    const vector<Descriptor::Info>& descriptor_infos) {
  size_t total_bindings = 0;
  for (const auto& info : descriptor_infos) {
    total_bindings += info.bindings.size();
  }

  vector<VkDescriptorSetLayoutBinding> layout_bindings;
  layout_bindings.reserve(total_bindings);
  for (const auto& info : descriptor_infos) {
    for (size_t i = 0; i < info.bindings.size(); ++i) {
      layout_bindings.emplace_back(VkDescriptorSetLayoutBinding{
          info.bindings[i].binding_point,
          info.descriptor_type,
          info.bindings[i].array_length,
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

void Descriptor::Init(const SharedContext& context,
                      const vector<Info>& infos) {
  context_ = context;
  pool_ = CreateDescriptorPool(context_, infos);
  layout_ = CreateDescriptorSetLayout(context_, infos);
  set_ = CreateDescriptorSet(context_, pool_, layout_);
}

void Descriptor::UpdateBufferInfos(
    const Info& descriptor_info,
    const vector<VkDescriptorBufferInfo>& buffer_infos) const {
  if (descriptor_info.bindings.size() != buffer_infos.size()) {
    throw std::runtime_error{"Failed to update image infos"};
  }

  vector<VkWriteDescriptorSet> write_desc_sets(buffer_infos.size());
  for (size_t i = 0; i < buffer_infos.size(); ++i) {
    write_desc_sets[i] = VkWriteDescriptorSet{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        /*pNext=*/nullptr,
        set_,
        descriptor_info.bindings[i].binding_point,
        /*dstArrayElement=*/0,  // target first descriptor in set
        /*descriptorCount=*/1,  // possible to update multiple descriptors
        descriptor_info.descriptor_type,
        /*pImageInfo=*/nullptr,
        &buffer_infos[i],
        /*pTexelBufferView=*/nullptr,
    };
  }
  vkUpdateDescriptorSets(*context_->device(), CONTAINER_SIZE(write_desc_sets),
                         write_desc_sets.data(), 0, nullptr);
}

void Descriptor::UpdateImageInfos(VkDescriptorType descriptor_type,
                                  const ImageInfos& image_infos) const {
  for (const auto& infos : image_infos) {
    VkWriteDescriptorSet write_desc_set{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        /*pNext=*/nullptr,
        set_,
        /*dstBinding=*/infos.first,
        /*dstArrayElement=*/0,  // target first descriptor in set
        CONTAINER_SIZE(infos.second),
        descriptor_type,
        infos.second.data(),
        /*pBufferInfo=*/nullptr,
        /*pTexelBufferView=*/nullptr,
    };
    vkUpdateDescriptorSets(*context_->device(), 1, &write_desc_set, 0, nullptr);
  }
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
