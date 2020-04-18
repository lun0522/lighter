//
//  descriptor.h
//
//  Created by Pujun Lun on 3/17/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_DESCRIPTOR_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_DESCRIPTOR_H

#include <functional>
#include <vector>

#include "jessie_steamer/common/model_loader.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// VkDescriptorSet bridges resources declared in shaders, and buffers and images
// that hold the actual data. It is allocated from VkDescriptorPool.
// It can be used across shaders, and we may use multiple descriptor sets in one
// shader. To be compatible with OpenGL, we will only use one descriptor set in
// each shader, but the user may now share descriptors across shaders to take
// advantage of Vulkan.
// This is the base class of all descriptor classes. The user should use it
// through derived classes. Since all descriptors need VkDescriptorSetLayout,
// which declares resources used in each binding point, it will be held and
// destroyed by this base class, and initialized by derived classes.
class Descriptor {
 public:
  using TextureType = common::ModelLoader::TextureType;

  // Maps a binding point to buffers bound to it.
  using BufferInfoMap = absl::flat_hash_map<
      uint32_t, std::vector<VkDescriptorBufferInfo>>;

  // Maps a binding point to images bound to it.
  using ImageInfoMap = absl::flat_hash_map<
      uint32_t, std::vector<VkDescriptorImageInfo>>;

  // Contains the information we need to define the layout of a descriptor set.
  struct Info {
    struct Binding {
      uint32_t binding_point;
      uint32_t array_length;
    };

    VkDescriptorType descriptor_type;
    VkShaderStageFlags shader_stage;
    std::vector<Binding> bindings;
  };

  // This class is neither copyable nor movable.
  Descriptor(const Descriptor&) = delete;
  Descriptor& operator=(const Descriptor&) = delete;

  virtual ~Descriptor() {
    vkDestroyDescriptorSetLayout(*context_->device(), layout_,
                                 *context_->allocator());
  }

  // Accessors.
  const VkDescriptorSetLayout& layout() const { return layout_; }

 protected:
  explicit Descriptor(SharedBasicContext context)
      : context_{std::move(FATAL_IF_NULL(context))} {}

  // Modifiers.
  void SetLayout(const VkDescriptorSetLayout& layout) { layout_ = layout; }

  // Pointer to context.
  const SharedBasicContext context_;

 private:
  // Opaque descriptor set layout object.
  VkDescriptorSetLayout layout_;
};

// This class creates descriptors that are updated only once during command
// buffer recording. UpdateBufferInfos() and UpdateImageInfos() should be called
// to relate the actual data to the descriptor before draw calls.
class StaticDescriptor : public Descriptor {
 public:
  // Declares the resources that are used in shaders and specified by 'infos'.
  // The descriptor set layout should be fixed once construction is done.
  StaticDescriptor(SharedBasicContext context, absl::Span<const Info> infos);

  // This class is neither copyable nor movable.
  StaticDescriptor(const StaticDescriptor&) = delete;
  StaticDescriptor& operator=(const StaticDescriptor&) = delete;

  ~StaticDescriptor() override {
    // Descriptor sets are implicitly cleaned up with the descriptor pool.
    vkDestroyDescriptorPool(*context_->device(), pool_, *context_->allocator());
  }

  // Relates the buffer data to this descriptor.
  const StaticDescriptor& UpdateBufferInfos(
      VkDescriptorType descriptor_type,
      const BufferInfoMap& buffer_info_map) const;

  // Relates the image data to this descriptor.
  const StaticDescriptor& UpdateImageInfos(
      VkDescriptorType descriptor_type,
      const ImageInfoMap& image_info_map) const;

  // Binds to this descriptor when 'command_buffer' is recording commands.
  void Bind(const VkCommandBuffer& command_buffer,
            const VkPipelineLayout& pipeline_layout,
            VkPipelineBindPoint pipeline_binding_point) const;

 private:
  // Relates the actual data to this descriptor.
  const StaticDescriptor& UpdateDescriptorSets(
      const std::vector<VkWriteDescriptorSet>& write_descriptor_sets) const;

  // Opaque descriptor pool object.
  VkDescriptorPool pool_;

  // Opaque descriptor set object.
  VkDescriptorSet set_;
};

// This class creates descriptors that can be updated multiple times during the
// command buffer recording. PushBufferInfos() and PushImageInfos() should be
// called to relate the actual data to the descriptor every time when different
// data need to be bound.
class DynamicDescriptor : public Descriptor {
 public:
  // Declares the resources that are used in shaders and specified by 'infos'.
  // The descriptor set layout should be fixed once construction is done.
  DynamicDescriptor(SharedBasicContext context, absl::Span<const Info> infos);

  // This class is neither copyable nor movable.
  DynamicDescriptor(const DynamicDescriptor&) = delete;
  DynamicDescriptor& operator=(const DynamicDescriptor&) = delete;

  // Relates the buffer data to this descriptor.
  const DynamicDescriptor& PushBufferInfos(
      const VkCommandBuffer& command_buffer,
      const VkPipelineLayout& pipeline_layout,
      VkPipelineBindPoint pipeline_binding_point,
      VkDescriptorType descriptor_type,
      const BufferInfoMap& buffer_info_map) const;

  // Relates the image data to this descriptor.
  const DynamicDescriptor& PushImageInfos(
      const VkCommandBuffer& command_buffer,
      const VkPipelineLayout& pipeline_layout,
      VkPipelineBindPoint pipeline_binding_point,
      VkDescriptorType descriptor_type,
      const ImageInfoMap& image_info_map) const;

 private:
  // Relates the actual data to this descriptor.
  const DynamicDescriptor& PushDescriptorSets(
      const VkCommandBuffer& command_buffer,
      const VkPipelineLayout& pipeline_layout,
      VkPipelineBindPoint pipeline_binding_point,
      const std::vector<VkWriteDescriptorSet>& write_descriptor_sets) const;

  // Function used to push descriptor sets.
  PFN_vkCmdPushDescriptorSetKHR push_descriptor_sets_func_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_DESCRIPTOR_H */
