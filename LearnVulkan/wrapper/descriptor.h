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
