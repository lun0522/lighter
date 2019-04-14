//
//  descriptor.h
//
//  Created by Pujun Lun on 3/17/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_DESCRIPTOR_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_DESCRIPTOR_H

#include <memory>
#include <vector>

#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

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
  struct Info {
    struct Subfield {
      uint32_t binding_point, array_length;
    };
    VkDescriptorType descriptor_type;
    VkShaderStageFlags shader_stage;
    std::vector<Subfield> subfields;
  };

  Descriptor() = default;
  void Init(std::shared_ptr<Context> context,
            const std::vector<Info>& infos);
  void UpdateBufferInfos(
      const Info& descriptor_info,
      const std::vector<VkDescriptorBufferInfo>& buffer_infos);
  void UpdateImageInfos(
      const Info& descriptor_info,
      const std::vector<std::vector<VkDescriptorImageInfo>>& image_infos);
  ~Descriptor();

  // This class is only movable
  Descriptor(Descriptor&&) = default;
  Descriptor& operator=(Descriptor&&) = default;

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
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_DESCRIPTOR_H */
