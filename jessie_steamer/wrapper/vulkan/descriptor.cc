//
//  descriptor.cc
//
//  Created by Pujun Lun on 3/17/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/descriptor.h"

#include <type_traits>

#include "jessie_steamer/wrapper/vulkan/util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

// Creates a descriptor pool, assuming it will only be used to allocate memory
// for one descriptor set.
VkDescriptorPool CreateDescriptorPool(
    const SharedBasicContext& context,
    absl::Span<const Descriptor::Info> descriptor_infos) {
  absl::flat_hash_map<VkDescriptorType, uint32_t> pool_size_map;
  for (const auto& info : descriptor_infos) {
    uint32_t total_length = 0;
    for (const auto& binding : info.bindings) {
      total_length += binding.array_length;
    }
    pool_size_map[info.descriptor_type] += total_length;
  }

  vector<VkDescriptorPoolSize> pool_sizes;
  pool_sizes.reserve(pool_size_map.size());
  for (const auto& pair : pool_size_map) {
    pool_sizes.emplace_back(VkDescriptorPoolSize{
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
  ASSERT_SUCCESS(vkCreateDescriptorPool(*context->device(), &pool_info,
                                        *context->allocator(), &pool),
                 "Failed to create descriptor pool");
  return pool;
}

// Creates a descriptor set layout. If 'is_dynamic' is true, the layout will be
// ready for pushing descriptors.
VkDescriptorSetLayout CreateDescriptorSetLayout(
    const SharedBasicContext& context,
    absl::Span<const Descriptor::Info> descriptor_infos,
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
  ASSERT_SUCCESS(vkCreateDescriptorSetLayout(*context->device(), &layout_info,
                                             *context->allocator(), &layout),
                 "Failed to create descriptor set layout");
  return layout;
}

// Allocates one descriptor set from 'pool' with the given 'layout'.
VkDescriptorSet AllocateDescriptorSet(const SharedBasicContext& context,
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
      vkAllocateDescriptorSets(*context->device(), &desc_set_info, &set),
      "Failed to allocate descriptor set");
  return set;
}

// Creates a list of VkWriteDescriptorSet for updating descriptor sets.
// 'info_map' maps each binding point to resources bound to it. The resource
// InfoType must be either VkDescriptorBufferInfo, VkDescriptorImageInfo or
// VkBufferView.
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
  for (const auto& pair : info_map) {
    const auto& info = pair.second;
    write_desc_sets.emplace_back(VkWriteDescriptorSet{
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
  pool_ = CreateDescriptorPool(context_, infos);
  layout_ = CreateDescriptorSetLayout(context_, infos, /*is_dynamic=*/false);
  set_ = AllocateDescriptorSet(context_, pool_, layout_);
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
    const vector<VkWriteDescriptorSet>& write_descriptor_sets) const {
  vkUpdateDescriptorSets(
      *context_->device(),
      CONTAINER_SIZE(write_descriptor_sets), write_descriptor_sets.data(),
      /*descriptorCopyCount=*/0, /*pDescriptorCopies=*/nullptr);
  return *this;
}

void StaticDescriptor::Bind(const VkCommandBuffer& command_buffer,
                            const VkPipelineLayout& pipeline_layout) const {
  vkCmdBindDescriptorSets(
      command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipeline_layout, /*firstSet=*/0, /*descriptorSetCount=*/1,
      &set_, /*dynamicOffsetCount=*/0, /*pDynamicOffsets=*/nullptr);
}

DynamicDescriptor::DynamicDescriptor(SharedBasicContext context,
                                     absl::Span<const Info> infos)
    : Descriptor{std::move(context)} {
  layout_ = CreateDescriptorSetLayout(context_, infos, /*is_dynamic=*/true);
  const auto vkCmdPushDescriptorSetKHR =
      util::LoadDeviceFunction<PFN_vkCmdPushDescriptorSetKHR>(
          *context_->device(), "vkCmdPushDescriptorSetKHR");
  push_descriptor_sets_ =
      [this, vkCmdPushDescriptorSetKHR](
          const VkCommandBuffer& command_buffer,
          const VkPipelineLayout& pipeline_layout,
          const vector<VkWriteDescriptorSet>& write_descriptor_sets)
              -> const DynamicDescriptor& {
        vkCmdPushDescriptorSetKHR(
            command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline_layout, /*set=*/0, CONTAINER_SIZE(write_descriptor_sets),
            write_descriptor_sets.data());
        return *this;
      };
}

const DynamicDescriptor& DynamicDescriptor::PushBufferInfos(
    const VkCommandBuffer& command_buffer,
    const VkPipelineLayout& pipeline_layout,
    VkDescriptorType descriptor_type,
    const BufferInfoMap& buffer_info_map) const {
  return push_descriptor_sets_(
      command_buffer, pipeline_layout,
      CreateWriteDescriptorSets(/*descriptor_set=*/VK_NULL_HANDLE,
                                descriptor_type, buffer_info_map));
}

const DynamicDescriptor& DynamicDescriptor::PushImageInfos(
    const VkCommandBuffer& command_buffer,
    const VkPipelineLayout& pipeline_layout,
    VkDescriptorType descriptor_type,
    const ImageInfoMap& image_info_map) const {
  return push_descriptor_sets_(
      command_buffer, pipeline_layout,
      CreateWriteDescriptorSets(/*descriptor_set=*/VK_NULL_HANDLE,
                                descriptor_type, image_info_map));
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
