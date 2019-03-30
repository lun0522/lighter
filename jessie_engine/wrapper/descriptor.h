//
//  descriptor.h
//
//  Created by Pujun Lun on 3/17/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef WRAPPER_VULKAN_DESCRIPTOR_H
#define WRAPPER_VULKAN_DESCRIPTOR_H

#include <vector>

#include <vulkan/vulkan.hpp>

namespace wrapper {
namespace vulkan {
namespace descriptor {

struct ResourceInfo {
  VkDescriptorType descriptor_type;
  std::vector<uint32_t> binding_points;
  VkShaderStageFlags shader_stage;
};

} /* namespace descriptor */

class Context;

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
 *    to a shader, here we can reuse descritor sets across different shaders.
 *    We can also use multiple descritor sets in one shader and use 'set = 1'
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
  Descriptor() = default;
  void Init(std::shared_ptr<Context> context,
            const std::vector<descriptor::ResourceInfo>& resource_infos);
  void UpdateBufferInfos(
      const descriptor::ResourceInfo& resource_info,
      const std::vector<VkDescriptorBufferInfo>& buffer_infos);
  void UpdateImageInfos(const descriptor::ResourceInfo& resource_info,
                        const std::vector<VkDescriptorImageInfo>& image_infos);
  ~Descriptor();

  // This class is neither copyable nor movable
  Descriptor(const Descriptor&) = delete;
  Descriptor& operator=(const Descriptor&) = delete;

  const VkDescriptorSetLayout& layout() const { return layout_; }
  const VkDescriptorSet& set()          const { return set_; }

 private:
  std::shared_ptr<Context> context_;
  VkDescriptorPool pool_;
  VkDescriptorSetLayout layout_;
  VkDescriptorSet set_;
};

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* WRAPPER_VULKAN_DESCRIPTOR_H */
