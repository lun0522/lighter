//
//  descriptor.cc
//
//  Created by Pujun Lun on 3/17/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/descriptor.h"

#include <type_traits>

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

VkDescriptorPool CreateDescriptorPool(
    const SharedBasicContext& context,
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
    const vector<Descriptor::Info>& descriptor_infos,
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
      CONTAINER_SIZE(layout_bindings),
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

template <typename InfoType>
vector<VkWriteDescriptorSet> CreateWriteDescriptorSets(
    const VkDescriptorSet& descriptor_set,
    VkDescriptorType descriptor_type,
    const absl::flat_hash_map<uint32_t, vector<InfoType>>& info_map) {
  static_assert(std::is_same<InfoType, VkDescriptorBufferInfo>::value ||
                    std::is_same<InfoType, VkDescriptorImageInfo>::value ||
                    std::is_same<InfoType, VkBufferView>::value,
                "Unexpected info type");

  using common::util::GetPointerIfTypeExpected;
  vector<VkWriteDescriptorSet> write_desc_sets;
  write_desc_sets.reserve(info_map.size());
  for (const auto& info : info_map) {
    write_desc_sets.emplace_back(VkWriteDescriptorSet{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        /*pNext=*/nullptr,
        descriptor_set,
        /*dstBinding=*/info.first,
        /*dstArrayElement=*/0,
        CONTAINER_SIZE(info.second),
        descriptor_type,
        /*pImageInfo=*/
        GetPointerIfTypeExpected<InfoType, VkDescriptorImageInfo>(info.second),
        /*pBufferInfo=*/
        GetPointerIfTypeExpected<InfoType, VkDescriptorBufferInfo>(info.second),
        /*pTexelBufferView=*/
        GetPointerIfTypeExpected<InfoType, VkBufferView>(info.second),
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

StaticDescriptor& StaticDescriptor::UpdateBufferInfos(
    VkDescriptorType descriptor_type,
    const BufferInfoMap& buffer_info_map) {
  UpdateDescriptorSets(
      CreateWriteDescriptorSets(set_, descriptor_type, buffer_info_map));
  return *this;
}

StaticDescriptor& StaticDescriptor::UpdateImageInfos(
    VkDescriptorType descriptor_type,
    const ImageInfoMap& image_info_map) {
  UpdateDescriptorSets(
      CreateWriteDescriptorSets(set_, descriptor_type, image_info_map));
  return *this;
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
  const auto vkCmdPushDescriptorSetKHR =
      util::LoadDeviceFunction<PFN_vkCmdPushDescriptorSetKHR>(
          *context_->device(), "vkCmdPushDescriptorSetKHR");
  push_descriptor_sets_ =
      [vkCmdPushDescriptorSetKHR](
          const VkCommandBuffer& command_buffer,
          const VkPipelineLayout& pipeline_layout,
          const vector<VkWriteDescriptorSet>& write_descriptor_sets) {
        vkCmdPushDescriptorSetKHR(
            command_buffer,
            /*pipelineBindPoint=*/VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline_layout, /*set=*/0, CONTAINER_SIZE(write_descriptor_sets),
            write_descriptor_sets.data());
      };
}

const DynamicDescriptor& DynamicDescriptor::PushBufferInfos(
    const VkCommandBuffer& command_buffer,
    const VkPipelineLayout& pipeline_layout,
    VkDescriptorType descriptor_type,
    const BufferInfoMap& buffer_info_map) const {
  push_descriptor_sets_(
      command_buffer, pipeline_layout,
      CreateWriteDescriptorSets(/*descriptor_set=*/VK_NULL_HANDLE,
                                descriptor_type, buffer_info_map));
  return *this;
}

const DynamicDescriptor& DynamicDescriptor::PushImageInfos(
    const VkCommandBuffer& command_buffer,
    const VkPipelineLayout& pipeline_layout,
    VkDescriptorType descriptor_type,
    const ImageInfoMap& image_info_map) const {
  push_descriptor_sets_(
      command_buffer, pipeline_layout,
      CreateWriteDescriptorSets(/*descriptor_set=*/VK_NULL_HANDLE,
                                descriptor_type, image_info_map));
  return *this;
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
