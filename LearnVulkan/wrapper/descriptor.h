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

class Context;

class Descriptor {
 public:
  Descriptor() = default;
  void Init(std::shared_ptr<Context> context,
            VkDescriptorType descriptor_type,
            const std::vector<uint32_t>& binding_points,
            VkShaderStageFlags shader_stage);
  void UpdateBufferInfos(
      const std::vector<VkDescriptorBufferInfo>& buffer_infos);
  ~Descriptor();

  // This class is neither copyable nor movable
  Descriptor(const Descriptor&) = delete;
  Descriptor& operator=(const Descriptor&) = delete;

  const std::vector<VkDescriptorSetLayout> layouts() const
      { return descriptor_set_layouts_; }
  const std::vector<VkDescriptorSet> sets() const { return descriptor_sets_; }

 private:
  std::shared_ptr<Context> context_;
  VkDescriptorPool descriptor_pool_;
  VkDescriptorType descriptor_type_;
  std::vector<uint32_t> binding_points_;
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts_;
  std::vector<VkDescriptorSet> descriptor_sets_;
};

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* WRAPPER_VULKAN_DESCRIPTOR_H */
