//
//  descriptor.h
//
//  Created by Pujun Lun on 3/17/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_DESCRIPTOR_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_DESCRIPTOR_H

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "jessie_steamer/common/model_loader.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

/** VkDescriptorPool allocates VkDescriptorSet objects.
 *
 *  Initialization:
 *    Maximum total amount of VkDescriptorSet objects that will be allocated
 *    List of VkDescriptorPoolSize objects (each of them sets that for a certain
 *      descriptor type, how many descriptors will be allocated)
 *
 *------------------------------------------------------------------------------
 *
 *  VkDescriptorSetLayoutBinding configures a binding point.
 *
 *  Initialization:
 *    Binding point
 *    Descriptor type (sampler, uniform buffer, storage buffer, etc.)
 *    Descriptor count (a uniform can be an array. this parameter specifies
 *      the length of the array)
 *    Shader stage (vertex, geometry, fragment, etc. or ALL_GRAPHICS to cover
 *      all graphics stages)
 *
 *------------------------------------------------------------------------------
 *
 *  VkDescriptorSetLayout contains an array of binding descriptions. Multiple
 *    descriptors can have the same layout, so we only need to pass this layout
 *    to the pipeline once. The pipeline requires a list of this kind of layouts
 *    during its initialization.
 *
 *  Initialization:
 *    List of VkDescriptorSetLayoutBinding objects
 *
 *------------------------------------------------------------------------------
 *
 *  VkDescriptorSet is the bridge between resources declared in the shader and
 *    buffers where we hold actual data. vkUpdateDescriptorSets will be called
 *    to build this connection. vkCmdBindDescriptorSets will be called to bind
 *    resources before a render call. Unlike OpenGL where resources are local
 *    to a shader, here we can reuse descriptor sets across different shaders.
 *    We can also use multiple descriptor sets in one shader and use 'set = 1'
 *    to specify from which set the data come from. However, OpenGL won't
 *    recognize this, so we will only use one set in one shader.
 *
 *  Initialization:
 *    VkDescriptorPool (which allocates space for it)
 *    VkDescriptorSetLayout
 *    Descriptor set count
 */
class Descriptor {
 public:
  // TODO: make ImageInfos a struct
  using ImageInfos = absl::flat_hash_map<
      uint32_t, std::vector<VkDescriptorImageInfo>>;
  using ResourceType = common::types::ResourceType;

  struct Info {
    struct Binding {
      ResourceType resource_type;
      uint32_t binding_point;
      uint32_t array_length;
    };

    VkDescriptorType descriptor_type;
    VkShaderStageFlags shader_stage;
    std::vector<Binding> bindings;
  };

  virtual ~Descriptor() {
    vkDestroyDescriptorSetLayout(*context_->device(), layout_,
                                 context_->allocator());
  }

  const VkDescriptorSetLayout& layout() const { return layout_; }
  const VkDescriptorSet& set()          const { return set_; }

 protected:
  explicit Descriptor(SharedBasicContext context)
      : context_{std::move(context)} {}

  SharedBasicContext context_;
  VkDescriptorSetLayout layout_;
  VkDescriptorSet set_;
};

class StaticDescriptor : public Descriptor {
 public:
  StaticDescriptor(SharedBasicContext context, const std::vector<Info>& infos);

  // This class is neither copyable nor movable.
  StaticDescriptor(const StaticDescriptor&) = delete;
  StaticDescriptor& operator=(const StaticDescriptor&) = delete;

  ~StaticDescriptor() override {
    vkDestroyDescriptorPool(*context_->device(), pool_, context_->allocator());
    // descriptor set is implicitly cleaned up with descriptor pool
  }

  void UpdateBufferInfos(
      const Info& descriptor_info,
      const std::vector<VkDescriptorBufferInfo>& buffer_infos) const;

  void UpdateImageInfos(VkDescriptorType descriptor_type,
                        const ImageInfos& image_infos) const;

 private:
  VkDescriptorPool pool_;
};

class DynamicDescriptor : public Descriptor {
 public:
  DynamicDescriptor(SharedBasicContext context, const std::vector<Info>& infos);

  // This class is neither copyable nor movable.
  DynamicDescriptor(const DynamicDescriptor&) = delete;
  DynamicDescriptor& operator=(const DynamicDescriptor&) = delete;

  void UpdateBufferInfos(
      const VkCommandBuffer& command_buffer,
      const VkPipelineLayout& pipeline_layout,
      const Info& descriptor_info,
      const std::vector<VkDescriptorBufferInfo>& buffer_infos) const;

  void UpdateImageInfos(const VkCommandBuffer& command_buffer,
                        const VkPipelineLayout& pipeline_layout,
                        VkDescriptorType descriptor_type,
                        const ImageInfos& image_infos) const;

 private:
  void UpdateDescriptorSet(
      const VkCommandBuffer& command_buffer,
      const VkPipelineLayout& pipeline_layout,
      const std::vector<VkWriteDescriptorSet>& write_descriptor_set) const;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_DESCRIPTOR_H */
