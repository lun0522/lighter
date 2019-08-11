//
//  descriptor.cc
//
//  Created by Pujun Lun on 3/17/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/descriptor.h"

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

VkDescriptorPool CreateDescriptorPool(
    const SharedBasicContext& context,
    const vector<StaticDescriptor::Info>& descriptor_infos) {
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
      /*flags=*/nullflag,
      /*maxSets=*/1,
      CONTAINER_SIZE(pool_sizes),
      pool_sizes.data(),
  };

  VkDescriptorPool pool;
  ASSERT_SUCCESS(vkCreateDescriptorPool(*context->device(), &pool_info,
                                        *context->allocator(), &pool),
                 "Failed to create descriptor pool");
  return pool;
}

VkDescriptorSetLayout CreateDescriptorSetLayout(
    const SharedBasicContext& context,
    const vector<StaticDescriptor::Info>& descriptor_infos,
    bool is_dynamic) {
  int total_bindings = 0;
  for (const auto& info : descriptor_infos) {
    total_bindings += info.bindings.size();
  }

  vector<VkDescriptorSetLayoutBinding> layout_bindings;
  layout_bindings.reserve(total_bindings);
  for (const auto& info : descriptor_infos) {
    for (int i = 0; i < info.bindings.size(); ++i) {
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
      /*flags=*/is_dynamic
                    ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR
                    : nullflag,
      /*bindingCount=*/CONTAINER_SIZE(layout_bindings),
      layout_bindings.data(),
  };

  VkDescriptorSetLayout layout;
  ASSERT_SUCCESS(vkCreateDescriptorSetLayout(*context->device(), &layout_info,
                                             *context->allocator(), &layout),
                 "Failed to create descriptor set layout");
  return layout;
}

VkDescriptorSet CreateDescriptorSet(const SharedBasicContext& context,
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

vector<VkWriteDescriptorSet> CreateBuffersWriteDescriptorSets(
    const VkDescriptorSet& descriptor_set,
    const Descriptor::Info& descriptor_info,
    const vector<VkDescriptorBufferInfo>& buffer_infos) {
  if (descriptor_info.bindings.size() != buffer_infos.size()) {
    FATAL("Failed to buffer write descriptor sets");
  }

  vector<VkWriteDescriptorSet> write_desc_sets;
  write_desc_sets.reserve(buffer_infos.size());
  for (int i = 0; i < buffer_infos.size(); ++i) {
    write_desc_sets.emplace_back(VkWriteDescriptorSet{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        /*pNext=*/nullptr,
        descriptor_set,
        descriptor_info.bindings[i].binding_point,
        /*dstArrayElement=*/0,  // target first descriptor in set
        /*descriptorCount=*/1,  // possible to update multiple descriptors
        descriptor_info.descriptor_type,
        /*pImageInfo=*/nullptr,
        &buffer_infos[i],
        /*pTexelBufferView=*/nullptr,
    });
  }
  return write_desc_sets;
}

vector<VkWriteDescriptorSet> CreateImagesWriteDescriptorSets(
    const VkDescriptorSet& descriptor_set,
    VkDescriptorType descriptor_type,
    const Descriptor::ImageInfos& image_infos) {
  vector<VkWriteDescriptorSet> write_desc_sets;
  write_desc_sets.reserve(image_infos.size());
  for (const auto& infos : image_infos) {
    write_desc_sets.emplace_back(VkWriteDescriptorSet{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        /*pNext=*/nullptr,
        descriptor_set,
        /*dstBinding=*/infos.first,
        /*dstArrayElement=*/0,  // target first descriptor in set
        CONTAINER_SIZE(infos.second),
        descriptor_type,
        infos.second.data(),
        /*pBufferInfo=*/nullptr,
        /*pTexelBufferView=*/nullptr,
    });
  }
  return write_desc_sets;
}

} /* namespace */

StaticDescriptor::StaticDescriptor(SharedBasicContext context,
                                   const vector<Info>& infos)
    : Descriptor{std::move(context)} {
  pool_ = CreateDescriptorPool(context_, infos);
  layout_ = CreateDescriptorSetLayout(context_, infos, /*is_dynamic=*/false);
  set_ = CreateDescriptorSet(context_, pool_, layout_);
}

void StaticDescriptor::UpdateBufferInfos(
    const Info& descriptor_info,
    const vector<VkDescriptorBufferInfo>& buffer_infos) const {
  UpdateDescriptorSets(
      CreateBuffersWriteDescriptorSets(set_, descriptor_info, buffer_infos));
}

void StaticDescriptor::UpdateImageInfos(VkDescriptorType descriptor_type,
                                  const ImageInfos& image_infos) const {
  UpdateDescriptorSets(
      CreateImagesWriteDescriptorSets(set_, descriptor_type, image_infos));
}

void StaticDescriptor::UpdateDescriptorSets(
    const vector<VkWriteDescriptorSet>& write_descriptor_sets) const {
  vkUpdateDescriptorSets(*context_->device(),
                         CONTAINER_SIZE(write_descriptor_sets),
                         write_descriptor_sets.data(), 0, nullptr);
}

void StaticDescriptor::Bind(const VkCommandBuffer& command_buffer,
                            const VkPipelineLayout& pipeline_layout) const {
  vkCmdBindDescriptorSets(
      command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipeline_layout, /*firstSet=*/0, /*descriptorSetCount=*/1,
      &set_, /*dynamicOffsetCount=*/0, /*pDynamicOffsets=*/nullptr);
}

DynamicDescriptor::DynamicDescriptor(SharedBasicContext context,
                                     const vector<Info>& infos)
    : Descriptor{std::move(context)} {
  layout_ = CreateDescriptorSetLayout(context_, infos, /*is_dynamic=*/true);
}

void DynamicDescriptor::PushBufferInfos(
    const VkCommandBuffer& command_buffer,
    const VkPipelineLayout& pipeline_layout,
    const Info& descriptor_info,
    const vector<VkDescriptorBufferInfo>& buffer_infos) const {
  PushDescriptorSets(
      command_buffer, pipeline_layout,
      CreateBuffersWriteDescriptorSets(/*descriptor_set=*/VK_NULL_HANDLE,
                                       descriptor_info, buffer_infos));
}

void DynamicDescriptor::PushImageInfos(
    const VkCommandBuffer& command_buffer,
    const VkPipelineLayout& pipeline_layout,
    VkDescriptorType descriptor_type,
    const ImageInfos& image_infos) const {
  PushDescriptorSets(
      command_buffer, pipeline_layout,
      CreateImagesWriteDescriptorSets(/*descriptor_set=*/VK_NULL_HANDLE,
                                      descriptor_type, image_infos));
}

void DynamicDescriptor::PushDescriptorSets(
    const VkCommandBuffer& command_buffer,
    const VkPipelineLayout& pipeline_layout,
    const vector<VkWriteDescriptorSet>& write_descriptor_sets) const {
  static const auto vkCmdPushDescriptorSetKHR =
      LoadDeviceFunction<PFN_vkCmdPushDescriptorSetKHR>(
          *context_->device(), "vkCmdPushDescriptorSetKHR");
  vkCmdPushDescriptorSetKHR(
      command_buffer, /*pipelineBindPoint=*/VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipeline_layout, /*set=*/0, CONTAINER_SIZE(write_descriptor_sets),
      write_descriptor_sets.data());
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
