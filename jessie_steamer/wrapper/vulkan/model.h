//
//  model.h
//
//  Created by Pujun Lun on 3/22/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H

#include <memory>
#include <string>
#include <vector>

#include "jessie_steamer/common/model_loader.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class Context;

class Model {
 public:
  struct Mesh {
    std::vector<TextureImage> textures;
  };

  Model() = default;

  // Uses light-weight obj file loader
  void Init(std::shared_ptr<Context> context,
            int obj_index_base,
            const std::string& obj_path,
            const std::vector<std::vector<std::string>>& tex_paths);

  // Uses Assimp for loading complex models
  void Init(std::shared_ptr<Context> context,
            const std::string& obj_path,
            const std::string& tex_path);

  void Draw(const VkCommandBuffer& command_buffer) const {
    vertex_buffer_.Draw(command_buffer);
  }

  // This class is neither copyable nor movable
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  static const std::vector<VkVertexInputBindingDescription>& binding_descs();
  static const std::vector<VkVertexInputAttributeDescription>& attrib_descs();
  void UpdateDescriptors(const std::vector<Descriptor::Info>& descriptor_infos,
                         std::vector<Descriptor>* descriptors);

 private:
  VertexBuffer vertex_buffer_;
  std::vector<Mesh> meshes_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H */
