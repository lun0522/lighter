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

#include "absl/types/optional.h"
#include "absl/types/variant.h"
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
  using TextureType = common::ModelLoader::Texture::Type;
  using Mesh = std::array<std::vector<std::unique_ptr<TextureImage>>,
                          TextureType::kTypeMaxEnum>;
  using UniformInfos = std::vector<std::pair<const UniformBuffer&,
                                             const Descriptor::Info&>>;

  // Textures that will be bound to the same point.
  struct TextureBinding {
    uint32_t binding_point;
    std::vector<std::vector<std::string>> texture_paths;
  };
  using BindingPointMap = std::unordered_map<TextureType, uint32_t,
                                             std::hash<int>>;
  using TextureBindingMap = std::unordered_map<TextureType, TextureBinding,
                                               std::hash<int>>;
  using FindBindingPoint = std::function<uint32_t(TextureType)>;

  // Loads with light-weight obj file loader.
  struct SingleMeshResource {
    std::string obj_path;
    unsigned int obj_index_base;
    TextureBindingMap binding_map;
  };
  // Loads with Assimp.
  struct MultiMeshResource {
    std::string obj_path;
    std::string tex_path;
    BindingPointMap binding_map;
  };
  using ModelResource = absl::variant<SingleMeshResource, MultiMeshResource>;

  // For instancing, caller must provide information about per-instance vertex
  // attributes.
  struct VertexAttribute {
    uint32_t location;
    uint32_t offset;
    VkFormat format;
  };
  struct InstancingInfo {
    std::vector<VertexAttribute> per_instance_attribs;
    uint32_t data_size;
    PerInstanceBuffer* per_instance_buffer;
  };

  Model() = default;

  // This class is neither copyable nor movable.
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  void Init(const std::shared_ptr<Context>& context,
            const std::vector<PipelineBuilder::ShaderInfo>& shader_infos,
            const ModelResource& resource,
            const absl::optional<UniformInfos>& uniform_infos,
            const absl::optional<InstancingInfo>& instancing_info,
            PushConstants* push_constants,
            size_t num_frame,
            bool is_opaque);
  void Draw(const VkCommandBuffer& command_buffer,
            size_t frame,
            uint32_t instance_count) const;

 private:
  FindBindingPoint LoadSingleMesh(const SingleMeshResource& resource);
  FindBindingPoint LoadMultiMesh(const MultiMeshResource& resource);
  void CreateDescriptors(const FindBindingPoint& find_binding_point,
                         const absl::optional<UniformInfos>& uniform_infos,
                         size_t num_frame);

  bool is_first_time_ = true;
  std::shared_ptr<Context> context_;
  PerVertexBuffer vertex_buffer_;
  std::vector<Mesh> meshes_;
  std::vector<std::vector<std::unique_ptr<Descriptor>>> descriptors_;
  // TODO: deal with shared resource in better way
  PushConstants* push_constants_ = nullptr;
  PerInstanceBuffer* per_instance_buffer_ = nullptr;
  PipelineBuilder pipeline_builder_;
  std::unique_ptr<Pipeline> pipeline_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H */
