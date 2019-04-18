//
//  model.h
//
//  Created by Pujun Lun on 3/22/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "jessie_steamer/common/model_loader.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class Context;

class Model {
 public:
  using TextureType = common::ModelLoader::Texture::Type;

  using UniformInfo = std::pair<const UniformBuffer*, const Descriptor::Info*>;

  struct Binding {
    uint32_t binding_point;
    std::vector<std::vector<std::string>> texture_paths;
  };
  using BindingMap = std::unordered_map<TextureType, Binding, std::hash<int>>;

  struct Mesh {
    std::array<std::vector<TextureImage>, TextureType::kTypeMaxEnum> textures;
  };

  Model() = default;

  // Uses light-weight obj file loader
  void Init(std::shared_ptr<Context> context,
            unsigned int obj_index_base,
            const std::string& obj_path,
            const BindingMap& bindings,
            const std::vector<UniformInfo>& uniform_infos,
            size_t num_frame);

  // Uses Assimp for loading complex models
  void Init(std::shared_ptr<Context> context,
            const std::string& obj_path,
            const std::string& tex_path,
            const BindingMap& bindings,
            const std::vector<UniformInfo>& uniform_infos,
            size_t num_frame);

  void Draw(const VkCommandBuffer& command_buffer) const {
    vertex_buffer_.Draw(command_buffer);
  }

  // This class is neither copyable nor movable
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  static const std::vector<VkVertexInputBindingDescription>& binding_descs();
  static const std::vector<VkVertexInputAttributeDescription>& attrib_descs();
  const Descriptor& descriptor(size_t frame) { return descriptors_[frame]; }

 private:
  VertexBuffer vertex_buffer_;
  std::vector<Mesh> meshes_;
  std::vector<Descriptor> descriptors_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H */
