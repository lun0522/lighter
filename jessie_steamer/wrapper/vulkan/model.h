//
//  model.h
//
//  Created by Pujun Lun on 3/22/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "jessie_steamer/common/model_loader.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class Context;

class Model {
 public:
  struct Range {
    size_t start, end;  // |end| is exclusive
  };

  struct TextureBinding {
    uint32_t binding_point;
    std::vector<std::vector<std::string>> texture_paths;
  };

  using TextureType = common::ModelLoader::Texture::Type;
  using Mesh = std::array<std::vector<TextureImage>, TextureType::kTypeMaxEnum>;
  using UniformInfo = std::pair<const UniformBuffer*, const Descriptor::Info*>;
  using BindingPointMap = std::unordered_map<TextureType, uint32_t,
                                             std::hash<int>>;
  using TextureBindingMap = std::unordered_map<TextureType, TextureBinding,
                                               std::hash<int>>;
  using FindBindingPoint = std::function<uint32_t(TextureType)>;

  Model() = default;

  // Uses light-weight obj file loader
  void Init(std::shared_ptr<Context> context,
            unsigned int obj_index_base,
            const std::string& obj_path,
            const TextureBindingMap& binding_map,
            const std::vector<UniformInfo>& uniform_infos,
            const std::vector<Pipeline::ShaderInfo>& shader_infos,
            size_t num_frame);

  // Uses Assimp for loading complex models
  void Init(std::shared_ptr<Context> context,
            const std::string& obj_path,
            const std::string& tex_path,
            const BindingPointMap& binding_map,
            const std::vector<UniformInfo>& uniform_infos,
            const std::vector<Pipeline::ShaderInfo>& shader_infos,
            size_t num_frame);

  void Cleanup();

  void Draw(const VkCommandBuffer& command_buffer,
            size_t frame) const;

  // This class is neither copyable nor movable
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

 private:
  class Drawable {
   public:
    Drawable(const std::shared_ptr<Context>& context,
             const Model& model,
             Range range,
             std::vector<std::unique_ptr<Descriptor>>&& descriptors);
    void Init();
    void Cleanup();

    // This class is neither copyable nor movable
    Drawable(const Drawable&) = delete;
    Drawable& operator=(const Drawable&) = delete;

    void Draw(const VkCommandBuffer& command_buffer,
              size_t frame) const;

    static std::vector<Range> GenRanges(
        const std::vector<common::ModelLoader::Mesh>& meshes,
        size_t max_num_sampler);

   private:
    std::shared_ptr<Context> context_;
    const Model& model_;
    Range range_;
    std::vector<std::unique_ptr<Descriptor>> descriptors_;
    Pipeline pipeline_;
  };

  bool is_first_time_{true};
  VertexBuffer vertex_buffer_;
  std::vector<Pipeline::ShaderInfo> shader_infos_;
  std::vector<Mesh> meshes_;
  std::vector<std::unique_ptr<Drawable>> drawables_;

  bool RunInit();

  void CreateDrawables(const std::shared_ptr<Context>& context,
                       const std::vector<Range>& ranges,
                       const std::vector<UniformInfo>& uniform_infos,
                       size_t num_frame,
                       const FindBindingPoint& find_binding_point);
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H */
