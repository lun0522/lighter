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
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "jessie_steamer/common/model_loader.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "jessie_steamer/wrapper/vulkan/vertex_input_util.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace model {

using TextureType = common::TextureType;
using TexPerMesh = std::array<std::vector<std::unique_ptr<SamplableImage>>,
                              static_cast<int>(TextureType::kNumType)>;

// For pushing constants.
struct PushConstantInfo {
  struct Info {
    const PushConstant* push_constant;
    uint32_t offset;
  };
  VkShaderStageFlags shader_stage;
  std::vector<Info> infos;
};

} /* namespace model */

class Model;

class ModelBuilder {
 public:
  using BufferInfoGenerator = std::function<VkDescriptorBufferInfo(int frame)>;

  // Textures of the same type will be bound to the same point.
  struct TextureBinding {
    using TextureSource = absl::variant<SharedTexture::SourcePath,
                                        OffscreenImagePtr>;
    uint32_t binding_point;
    std::vector<TextureSource> texture_sources;
  };
  using BindingPointMap = absl::flat_hash_map<
      model::TextureType, uint32_t, common::util::EnumClassHash>;
  using TextureBindingMap = absl::flat_hash_map<
      model::TextureType, TextureBinding, common::util::EnumClassHash>;

  // Loads with light-weight obj file loader.
  struct SingleMeshResource {
    std::string obj_path;
    int obj_index_base;
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
  struct InstancingInfo {
    std::vector<VertexAttribute> per_instance_attribs;
    uint32_t data_size;
    const PerInstanceBuffer* per_instance_buffer;
  };

  ModelBuilder(SharedBasicContext context,
               int num_frame, bool is_opaque, const ModelResource& resource);

  // This class is neither copyable nor movable.
  ModelBuilder(const ModelBuilder&) = delete;
  ModelBuilder& operator=(const ModelBuilder&) = delete;

  // Should be called after all setters are called. Note that all internal
  // states will be invalid after calling Build(), and the caller should discard
  // the builder and perform all updates on the Model instance.
  std::unique_ptr<Model> Build();

  ModelBuilder& add_shader(PipelineBuilder::ShaderInfo&& info);
  ModelBuilder& add_instancing(InstancingInfo&& info);
  ModelBuilder& add_uniform_usage(Descriptor::Info&& info);
  ModelBuilder& add_uniform_resource(uint32_t binding_point,
                                     const BufferInfoGenerator& info_gen);
  ModelBuilder& set_push_constant(model::PushConstantInfo&& info);
  ModelBuilder& add_shared_texture(model::TextureType type,
                                   const TextureBinding& binding);

 private:
  void LoadSingleMesh(const SingleMeshResource& resource);
  void LoadMultiMesh(const MultiMeshResource& resource);
  std::vector<std::vector<std::unique_ptr<StaticDescriptor>>>
  CreateDescriptors();

  const SharedBasicContext context_;
  const int num_frame_;
  std::vector<PipelineBuilder::ShaderInfo> shader_infos_;
  std::unique_ptr<StaticPerVertexBuffer> vertex_buffer_;
  std::vector<InstancingInfo> instancing_infos_;
  std::vector<Descriptor::Info> uniform_usages_;
  std::vector<Descriptor::BufferInfoMap> uniform_resource_maps_;
  absl::optional<model::PushConstantInfo> push_constant_info_;
  BindingPointMap binding_map_;
  model::TexPerMesh shared_textures_;
  std::vector<model::TexPerMesh> mesh_textures_;
  std::unique_ptr<PipelineBuilder> pipeline_builder_;
};

class Model {
 public:
  // This class is neither copyable nor movable.
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  // Should be called after initialization and whenever frame is resized.
  void Update(VkExtent2D frame_size, VkSampleCountFlagBits sample_count,
              const RenderPass& render_pass, uint32_t subpass_index);

  void Draw(const VkCommandBuffer& command_buffer,
            int frame, uint32_t instance_count) const;

 private:
  friend std::unique_ptr<Model> ModelBuilder::Build();

  Model(SharedBasicContext context,
        std::vector<PipelineBuilder::ShaderInfo>&& shader_infos,
        std::unique_ptr<StaticPerVertexBuffer>&& vertex_buffer,
        std::vector<const PerInstanceBuffer*>&& per_instance_buffers,
        absl::optional<model::PushConstantInfo>&& push_constant_info,
        model::TexPerMesh&& shared_textures,
        std::vector<model::TexPerMesh>&& mesh_textures,
        std::vector<std::vector<std::unique_ptr<StaticDescriptor>>>&&
        descriptors,
        std::unique_ptr<PipelineBuilder>&& pipeline_builder)
      : context_{std::move(context)},
        shader_infos_{std::move(shader_infos)},
        vertex_buffer_{std::move(vertex_buffer)},
        per_instance_buffers_{std::move(per_instance_buffers)},
        push_constant_info_{std::move(push_constant_info)},
        shared_textures_{std::move(shared_textures)},
        mesh_textures_{std::move(mesh_textures)},
        descriptors_{std::move(descriptors)},
        pipeline_builder_{std::move(pipeline_builder)} {}

  const SharedBasicContext context_;
  const std::vector<PipelineBuilder::ShaderInfo> shader_infos_;
  const std::unique_ptr<StaticPerVertexBuffer> vertex_buffer_;
  const std::vector<const PerInstanceBuffer*> per_instance_buffers_;
  const absl::optional<model::PushConstantInfo> push_constant_info_;
  const model::TexPerMesh shared_textures_;
  const std::vector<model::TexPerMesh> mesh_textures_;
  const std::vector<std::vector<std::unique_ptr<StaticDescriptor>>>
      descriptors_;
  std::unique_ptr<PipelineBuilder> pipeline_builder_;
  std::unique_ptr<Pipeline> pipeline_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H */
