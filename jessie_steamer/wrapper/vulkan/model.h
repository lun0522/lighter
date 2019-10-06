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

#include "jessie_steamer/common/model_loader.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/variant.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// Forward declarations.
class Model;

class ModelBuilder {
 public:
  using TextureType = common::ModelLoader::TextureType;

  // Each mesh can have any type of textures, and a list of samplable images
  // in each type.
  using TexturesPerMesh =
      std::array<std::vector<std::unique_ptr<SamplableImage>>,
                 static_cast<int>(TextureType::kNumTypes)>;

  // Each element is the descriptor used by the mesh at the same index.
  using DescriptorsPerFrame = std::vector<std::unique_ptr<StaticDescriptor>>;

  // Textures of the same type will be bound to the same point.
  struct TextureBinding {
    using TextureSource = absl::variant<SharedTexture::SourcePath,
                                        OffscreenImagePtr>;
    uint32_t binding_point;
    std::vector<TextureSource> texture_sources;
  };
  using BindingPointMap = absl::flat_hash_map<
      TextureType, uint32_t, common::util::EnumClassHash>;
  using TextureBindingMap = absl::flat_hash_map<
      TextureType, TextureBinding, common::util::EnumClassHash>;

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
  struct PerInstanceBufferInfo {
    std::vector<pipeline::VertexInputAttribute::Attribute> per_instance_attribs;
    uint32_t data_size;
    const PerInstanceBuffer* per_instance_buffer;
  };

  // Contains information for pushing constants. We assume that in each frame,
  // for each PushConstant, all the data prepared for one frame will be sent to
  // the device, written with 'offset' bytes, and consumed in 'shader_stage'.
  struct PushConstantInfos {
    struct Info {
      const PushConstant* push_constant;
      uint32_t target_offset;
    };

    VkShaderStageFlags shader_stage;
    std::vector<Info> infos;
  };

  ModelBuilder(SharedBasicContext context,
               int num_frames, const ModelResource& resource);

  // This class is neither copyable nor movable.
  ModelBuilder(const ModelBuilder&) = delete;
  ModelBuilder& operator=(const ModelBuilder&) = delete;

  // Adds textures shared by all meshes, such as the skybox texture.
  // Note that if ModelResource contains any mesh textures of the same type,
  // 'binding.binding_point' must be the same to their binding point.
  ModelBuilder& AddSharedTextures(TextureType type,
                                  const TextureBinding& binding);

  // Adds a per-instance vertex buffer.
  ModelBuilder& AddPerInstanceBuffer(PerInstanceBufferInfo&& info);

  // Declares how many uniform data should be expected at each binding point.
  ModelBuilder& AddUniformBinding(
      VkShaderStageFlags shader_stage,
      std::vector<Descriptor::Info::Binding>&& bindings);

  // Binds 'uniform_buffer' to 'binding_point'. The user may bind multiple
  // buffers to one point.
  ModelBuilder& AddUniformBuffer(uint32_t binding_point,
                                 const UniformBuffer& uniform_buffer);

  // Sets pushed constants will be used in which shader stages.
  ModelBuilder& SetPushConstantShaderStage(VkShaderStageFlags shader_stage);

  // Adds a push constant data source. The user is responsible for keeping the
  // existence of 'push_constant'.
  ModelBuilder& AddPushConstant(const PushConstant* push_constant,
                                uint32_t target_offset);

  // Adds a shader. The instance of Model will store the shader information,
  // hence unlike PipelineBuilder, the user do not need to add shaders again.
  ModelBuilder& AddShader(PipelineBuilder::ShaderInfo&& info);

  // Should be called after all setters are called. Note that all internal
  // states will be invalid after calling Build(), and the caller should discard
  // the builder and perform all updates on the Model instance.
  std::unique_ptr<Model> Build();

 private:
  // Loads meshes and populates 'vertex_buffer_', 'binding_map_' and
  // 'mesh_textures_'.
  void LoadSingleMesh(const SingleMeshResource& resource);
  void LoadMultiMesh(const MultiMeshResource& resource);

  // Creates descriptors for all resources used for rendering the model.
  std::vector<DescriptorsPerFrame> CreateDescriptors() const;

  // Pointer to context.
  const SharedBasicContext context_;

  // Number of frames.
  const int num_frames_;

  std::unique_ptr<StaticPerVertexBuffer> vertex_buffer_;
  BindingPointMap binding_map_;

  // Each element stores textures for the mesh at the same index.
  std::vector<TexturesPerMesh> mesh_textures_;

  // Stores the textures shared by all meshes.
  TexturesPerMesh shared_textures_;

  std::vector<PerInstanceBufferInfo> per_instance_buffer_infos_;
  std::vector<Descriptor::Info> uniform_descriptor_infos_;
  // Elements are indexed by the frame, and the length of this should be equal
  // to 'num_frames_'. Each element maps binding points to buffer infos of the
  // uniform buffers bound to them.
  std::vector<Descriptor::BufferInfoMap> uniform_buffer_info_maps_;
  absl::optional<PushConstantInfos> push_constant_infos_;
  std::vector<PipelineBuilder::ShaderInfo> shader_infos_;
};

class Model {
 public:
  // This class is neither copyable nor movable.
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  // Should be called after initialization and whenever frame is resized.
  // If the object is opaque, depth testing will be enabled.
  void Update(bool is_opaque,
              const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
              const RenderPass& render_pass, uint32_t subpass_index);

  void Draw(const VkCommandBuffer& command_buffer,
            int frame, uint32_t instance_count) const;

 private:
  using DescriptorsPerFrame = ModelBuilder::DescriptorsPerFrame;
  using PushConstantInfos = ModelBuilder::PushConstantInfos;
  using TexturesPerMesh = ModelBuilder::TexturesPerMesh;

  friend std::unique_ptr<Model> ModelBuilder::Build();

  Model(SharedBasicContext context,
        std::vector<PipelineBuilder::ShaderInfo>&& shader_infos,
        std::unique_ptr<StaticPerVertexBuffer>&& vertex_buffer,
        std::vector<const PerInstanceBuffer*>&& per_instance_buffers,
        absl::optional<PushConstantInfos>&& push_constant_info,
        TexturesPerMesh&& shared_textures,
        std::vector<TexturesPerMesh>&& mesh_textures,
        std::vector<DescriptorsPerFrame>&& descriptors,
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
  const absl::optional<PushConstantInfos> push_constant_info_;
  const TexturesPerMesh shared_textures_;
  const std::vector<TexturesPerMesh> mesh_textures_;
  const std::vector<DescriptorsPerFrame> descriptors_;
  std::unique_ptr<PipelineBuilder> pipeline_builder_;
  std::unique_ptr<Pipeline> pipeline_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H */
