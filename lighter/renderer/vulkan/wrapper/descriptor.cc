//
//  descriptor.cc
//
//  Created by Pujun Lun on 3/17/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/descriptor.h"

#include <type_traits>

#include "lighter/renderer/vulkan/wrapper/util.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

// Creates a descriptor pool, assuming it will only be used to allocate memory
// for one descriptor set.
VkDescriptorPool CreateDescriptorPool(
    const BasicContext& context,
    absl::Span<const Descriptor::Info> descriptor_infos) {
  absl::flat_hash_map<VkDescriptorType, uint32_t> pool_size_map;
  for (const auto& info : descriptor_infos) {
    uint32_t total_length = 0;
    for (const auto& binding : info.bindings) {
      total_length += binding.array_length;
    }
    pool_size_map[info.descriptor_type] += total_length;
  }

  std::vector<VkDescriptorPoolSize> pool_sizes;
  pool_sizes.reserve(pool_size_map.size());
  for (const auto& pair : pool_size_map) {
    pool_sizes.push_back(VkDescriptorPoolSize{
        /*type=*/pair.first,
        /*descriptorCount=*/pair.second,
    });
  }

  const VkDescriptorPoolCreateInfo pool_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      /*maxSets=*/1,
      CONTAINER_SIZE(pool_sizes),
      pool_sizes.data(),
  };

  VkDescriptorPool pool;
  ASSERT_SUCCESS(vkCreateDescriptorPool(*context.device(), &pool_info,
                                        *context.allocator(), &pool),
                 "Failed to create descriptor pool");
  return pool;
}

// Creates a descriptor set layout. If 'is_dynamic' is true, the layout will be
// ready for pushing descriptors.
VkDescriptorSetLayout CreateDescriptorSetLayout(
    const BasicContext& context,
    absl::Span<const Descriptor::Info> descriptor_infos,
    bool is_dynamic) {
  int total_bindings = 0;
  for (const auto& info : descriptor_infos) {
    total_bindings += info.bindings.size();
  }

  std::vector<VkDescriptorSetLayoutBinding> layout_bindings;
  layout_bindings.reserve(total_bindings);
  for (const auto& info : descriptor_infos) {
    for (int i = 0; i < info.bindings.size(); ++i) {
      layout_bindings.push_back(VkDescriptorSetLayoutBinding{
          info.bindings[i].binding_point,
          info.descriptor_type,
          info.bindings[i].array_length,
          info.shader_stage,
          /*pImmutableSamplers=*/nullptr,
      });
    }
  }

  const VkDescriptorSetLayoutCreateInfo layout_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/
      is_dynamic ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR
                 : nullflag,
      CONTAINER_SIZE(layout_bindings),
      layout_bindings.data(),
  };

  VkDescriptorSetLayout layout;
  ASSERT_SUCCESS(vkCreateDescriptorSetLayout(*context.device(), &layout_info,
                                             *context.allocator(), &layout),
                 "Failed to create descriptor set layout");
  return layout;
}

// Allocates one descriptor set from 'pool' with the given 'layout'.
VkDescriptorSet AllocateDescriptorSet(const BasicContext& context,
                                      const VkDescriptorPool& pool,
                                      const VkDescriptorSetLayout& layout) {
  const VkDescriptorSetAllocateInfo desc_set_info{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      pool,
      /*descriptorSetCount=*/1,
      &layout,
  };

  VkDescriptorSet set;
  ASSERT_SUCCESS(
      vkAllocateDescriptorSets(*context.device(), &desc_set_info, &set),
      "Failed to allocate descriptor set");
  return set;
}

// Creates a vector of VkWriteDescriptorSet for updating descriptor sets.
// 'info_map' maps each binding point to resources bound to it. The resource
// InfoType must be either VkDescriptorBufferInfo, VkDescriptorImageInfo or
// VkBufferView.
template <typename InfoType>
std::vector<VkWriteDescriptorSet> CreateWriteDescriptorSets(
    const VkDescriptorSet& descriptor_set,
    VkDescriptorType descriptor_type,
    const absl::flat_hash_map<uint32_t, std::vector<InfoType>>& info_map) {
  static_assert(std::is_same_v<InfoType, VkDescriptorBufferInfo> ||
                    std::is_same_v<InfoType, VkDescriptorImageInfo> ||
                    std::is_same_v<InfoType, VkBufferView>,
                "Unexpected info type");

  using common::util::GetPointerIfTypeExpected;
  std::vector<VkWriteDescriptorSet> write_desc_sets;
  write_desc_sets.reserve(info_map.size());
  for (const auto& pair : info_map) {
    const auto& info = pair.second;
    write_desc_sets.push_back(VkWriteDescriptorSet{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        /*pNext=*/nullptr,
        descriptor_set,
        /*dstBinding=*/pair.first,
        /*dstArrayElement=*/0,
        CONTAINER_SIZE(info),
        descriptor_type,
        /*pImageInfo=*/GetPointerIfTypeExpected<VkDescriptorImageInfo>(info),
        /*pBufferInfo=*/GetPointerIfTypeExpected<VkDescriptorBufferInfo>(info),
        /*pTexelBufferView=*/GetPointerIfTypeExpected<VkBufferView>(info),
    });
  }
  return write_desc_sets;
}

} /* namespace */

StaticDescriptor::StaticDescriptor(SharedBasicContext context,
                                   absl::Span<const Info> infos)
    : Descriptor{std::move(context)} {
  pool_ = CreateDescriptorPool(*context_, infos);
  const auto layout = CreateDescriptorSetLayout(*context_, infos,
                                                /*is_dynamic=*/false);
  set_layout(layout);
  set_ = AllocateDescriptorSet(*context_, pool_, layout);
}

const StaticDescriptor& StaticDescriptor::UpdateBufferInfos(
    VkDescriptorType descriptor_type,
    const BufferInfoMap& buffer_info_map) const {
  return UpdateDescriptorSets(
      CreateWriteDescriptorSets(set_, descriptor_type, buffer_info_map));
}

const StaticDescriptor& StaticDescriptor::UpdateImageInfos(
    VkDescriptorType descriptor_type,
    const ImageInfoMap& image_info_map) const {
  return UpdateDescriptorSets(
      CreateWriteDescriptorSets(set_, descriptor_type, image_info_map));
}

const StaticDescriptor& StaticDescriptor::UpdateDescriptorSets(
    const std::vector<VkWriteDescriptorSet>& write_descriptor_sets) const {
  vkUpdateDescriptorSets(
      *context_->device(),
      CONTAINER_SIZE(write_descriptor_sets), write_descriptor_sets.data(),
      /*descriptorCopyCount=*/0, /*pDescriptorCopies=*/nullptr);
  return *this;
}

void StaticDescriptor::Bind(const VkCommandBuffer& command_buffer,
                            const VkPipelineLayout& pipeline_layout,
                            VkPipelineBindPoint pipeline_binding_point) const {
  vkCmdBindDescriptorSets(
      command_buffer, pipeline_binding_point, pipeline_layout,
      /*firstSet=*/0, /*descriptorSetCount=*/1, &set_, /*dynamicOffsetCount=*/0,
      /*pDynamicOffsets=*/nullptr);
}

DynamicDescriptor::DynamicDescriptor(SharedBasicContext context,
                                     absl::Span<const Info> infos)
    : Descriptor{std::move(context)} {
  set_layout(CreateDescriptorSetLayout(*context_, infos, /*is_dynamic=*/true));
  push_descriptor_sets_func_ =
      util::LoadDeviceFunction<PFN_vkCmdPushDescriptorSetKHR>(
          *context_->device(), "vkCmdPushDescriptorSetKHR");
}

const DynamicDescriptor& DynamicDescriptor::PushBufferInfos(
    const VkCommandBuffer& command_buffer,
    const VkPipelineLayout& pipeline_layout,
    VkPipelineBindPoint pipeline_binding_point,
    VkDescriptorType descriptor_type,
    const BufferInfoMap& buffer_info_map) const {
  return PushDescriptorSets(
      command_buffer, pipeline_layout, pipeline_binding_point,
      CreateWriteDescriptorSets(/*descriptor_set=*/VK_NULL_HANDLE,
                                descriptor_type, buffer_info_map));
}

const DynamicDescriptor& DynamicDescriptor::PushImageInfos(
    const VkCommandBuffer& command_buffer,
    const VkPipelineLayout& pipeline_layout,
    VkPipelineBindPoint pipeline_binding_point,
    VkDescriptorType descriptor_type,
    const ImageInfoMap& image_info_map) const {
  return PushDescriptorSets(
      command_buffer, pipeline_layout, pipeline_binding_point,
      CreateWriteDescriptorSets(/*descriptor_set=*/VK_NULL_HANDLE,
                                descriptor_type, image_info_map));
}

const DynamicDescriptor& DynamicDescriptor::PushDescriptorSets(
    const VkCommandBuffer& command_buffer,
    const VkPipelineLayout& pipeline_layout,
    VkPipelineBindPoint pipeline_binding_point,
    const std::vector<VkWriteDescriptorSet>& write_descriptor_sets) const {
  push_descriptor_sets_func_(
      command_buffer, pipeline_binding_point, pipeline_layout, /*set=*/0,
      CONTAINER_SIZE(write_descriptor_sets), write_descriptor_sets.data());
  return *this;
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
