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

// The user should use this class to create Model. After calling Build(),
// internal states will be invalidated, and the builder should be discarded.
class ModelBuilder {
 public:
  using TextureType = common::ModelLoader::TextureType;

  // Each mesh can have any type of textures, and a list of samplable images
  // of each type. This array is indexed by the texture type.
  // The order of textures within in each TexturesPerMesh is assumed to be the
  // same to the order in shaders.
  using TexturesPerMesh =
      std::array<std::vector<std::unique_ptr<SamplableImage>>,
                 static_cast<int>(TextureType::kNumTypes)>;

  // Maps each texture type to its binding point.
  using BindingPointMap = absl::flat_hash_map<
      TextureType, uint32_t, common::util::EnumClassHash>;

  // Textures are either loaded from files or existing offscreen images.
  using TextureSource = absl::variant<SharedTexture::SourcePath,
                                      OffscreenImagePtr>;

  // Maps each texture type to textures of this type.
  using TextureSourceMap = absl::flat_hash_map<
      TextureType, std::vector<TextureSource>, common::util::EnumClassHash>;

  // Contains information required for loading one mesh from the Wavefront .obj
  // file at 'obj_path' and textures in 'tex_source_map' using a light-weight
  // .obj file loader.
  struct SingleMeshResource {
    std::string obj_path;
    int obj_file_index_base;
    TextureSourceMap tex_source_map;
  };
  // Contains information required for loading the model from 'model_path' and
  // textures from 'texture_dir' using Assimp.
  struct MultiMeshResource {
    std::string model_path;
    std::string texture_dir;
  };
  using ModelResource = absl::variant<SingleMeshResource, MultiMeshResource>;

  // TODO: Put these info in PerInstanceBuffer.
  // Contains information for instancing.
  struct PerInstanceBufferInfo {
    std::vector<pipeline::VertexInputAttribute::Attribute>
        vertex_input_attributes;
    uint32_t per_instance_data_size;
    const PerInstanceBuffer* per_instance_buffer;
  };

  // Contains information for pushing constants. We assume that in each frame,
  // for each PushConstant, PushConstant::size_per_frame() bytes will be sent to
  // the device, written with 'target_offset' bytes, and used in 'shader_stage'.
  struct PushConstantInfos {
    struct Info {
      const PushConstant* push_constant;
      uint32_t target_offset;
    };

    VkShaderStageFlags shader_stage;
    std::vector<Info> infos;
  };

  // Each element is the descriptor used by the mesh at the same index.
  using DescriptorsPerFrame = std::vector<std::unique_ptr<StaticDescriptor>>;

  // Specifies a shader resource.
  using ShaderInfo = std::pair</*shader_stage*/VkShaderStageFlagBits,
                               /*file_path*/std::string>;

  // If any offscreen images are used in 'resource', the user is responsible for
  // keeping the existence of them.
  ModelBuilder(SharedBasicContext context,
               int num_frames_in_flight, const ModelResource& resource);

  // This class is neither copyable nor movable.
  ModelBuilder(const ModelBuilder&) = delete;
  ModelBuilder& operator=(const ModelBuilder&) = delete;

  // Adds a texture shared by all meshes, such as the texture of skybox.
  ModelBuilder& AddSharedTexture(TextureType type,
                                 const TextureSource& texture_source);

  // Binds all textures of 'type' to 'binding_point'.
  // This can be called before or after textures of 'type' are added, since the
  // value would not be used until calling Build().
  ModelBuilder& AddTextureBindingPoint(TextureType type,
                                       uint32_t binding_point);

  // Adds a per-instance vertex buffer.
  // The user is responsible for keeping the existence of the buffer.
  ModelBuilder& AddPerInstanceBuffer(PerInstanceBufferInfo&& info);

  // Declares how many uniform data should be expected at each binding point.
  ModelBuilder& AddUniformBinding(
      VkShaderStageFlags shader_stage,
      std::vector<Descriptor::Info::Binding>&& bindings);

  // Binds 'uniform_buffer' to 'binding_point'.
  // The user may bind multiple buffers to one point.
  ModelBuilder& AddUniformBuffer(uint32_t binding_point,
                                 const UniformBuffer& uniform_buffer);

  // Sets pushed constants will be used in which shader stages.
  ModelBuilder& SetPushConstantShaderStage(VkShaderStageFlags shader_stage);

  // Adds a push constant data source.
  // The user is responsible for keeping the existence of 'push_constant'.
  ModelBuilder& AddPushConstant(const PushConstant* push_constant,
                                uint32_t target_offset);

  // Adds a shader. The instance of Model will store the shader information,
  // hence unlike PipelineBuilder, the user only need to add shaders once here.
  ModelBuilder& AddShader(VkShaderStageFlagBits shader_stage,
                          std::string&& file_path);

  // Returns a model. All internal states will be invalidated after this.
  // The user should discard the builder, and perform future updates on the
  // returned model directly.
  std::unique_ptr<Model> Build();

 private:
  // Loads meshes and populates 'vertex_buffer_' and 'mesh_textures_'.
  void LoadSingleMesh(const SingleMeshResource& resource);
  void LoadMultiMesh(const MultiMeshResource& resource);

  // Creates descriptors for all resources used for rendering the model.
  std::vector<DescriptorsPerFrame> CreateDescriptors() const;

  // Pointer to context.
  const SharedBasicContext context_;

  // Number of frames in flight.
  const int num_frames_in_flight_;

  // Holds per-vertex data.
  std::unique_ptr<StaticPerVertexBuffer> vertex_buffer_;

  // Each element stores textures used for the mesh at the same index.
  std::vector<TexturesPerMesh> mesh_textures_;

  // Textures shared by all meshes.
  TexturesPerMesh shared_textures_;

  // Maps each texture type to its binding point.
  BindingPointMap texture_binding_map_;

  // Describes the usage of per-instance vertex buffers.
  std::vector<PerInstanceBufferInfo> per_instance_buffer_infos_;

  // Declares uniform data used in shaders.
  std::vector<Descriptor::Info> uniform_descriptor_infos_;

  // Each element maps binding points to buffer infos of the uniform buffers
  // bound to them. Elements are indexed by the frame, and the length of this
  // should be equal to 'num_frames_in_flight_'.
  std::vector<Descriptor::BufferInfoMap> uniform_buffer_info_maps_;

  // Describes push constant data sources.
  absl::optional<PushConstantInfos> push_constant_infos_;

  // Shaders used in the graphics pipeline.
  std::vector<ShaderInfo> shader_infos_;
};

// The Model and its builder class are used to:
//   - Load and bind per-vertex data and textures.
//   - Bind vertex buffers (used for instancing), uniform buffers and push
//     constants.
//   - Load shaders of all stages.
//   - Maintain a graphics pipeline internally, and render the model during
//     command buffer recordings.
// The user should use ModelBuilder to create instances of this class.
// Update() should be called before calling Draw() for the first time, and
// whenever the render pass is changed, or if the user wants to change the
// transparency of the object.
class Model {
 public:
  // This class is neither copyable nor movable.
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  // TODO: Transparency can be changed via dynamic states.
  // Updates internal states and rebuilds the graphics pipeline.
  // For simplicity, the render area will be the same to 'frame_size'.
  // This must have been called once before calling Draw() for the first time.
  void Update(bool is_object_opaque,
              const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
              const RenderPass& render_pass, uint32_t subpass_index);

  // Renders the model.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer,
            int frame, uint32_t instance_count) const;

 private:
  using DescriptorsPerFrame = ModelBuilder::DescriptorsPerFrame;
  using PushConstantInfos = ModelBuilder::PushConstantInfos;
  using TexturesPerMesh = ModelBuilder::TexturesPerMesh;

  friend std::unique_ptr<Model> ModelBuilder::Build();

  Model(SharedBasicContext context,
        std::vector<ModelBuilder::ShaderInfo>&& shader_infos,
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

  // Pointer to context.
  const SharedBasicContext context_;

  // Shaders used in the graphics pipeline.
  const std::vector<ModelBuilder::ShaderInfo> shader_infos_;

  // Holds per-vertex data.
  const std::unique_ptr<StaticPerVertexBuffer> vertex_buffer_;

  // Stores per-instance vertex data.
  const std::vector<const PerInstanceBuffer*> per_instance_buffers_;

  // Describes push constant data sources.
  const absl::optional<PushConstantInfos> push_constant_info_;

  // Textures shared by all meshes.
  const TexturesPerMesh shared_textures_;

  // Each element stores textures used for the mesh at the same index.
  const std::vector<TexturesPerMesh> mesh_textures_;

  // Each element is the descriptor used for the mesh at the same index.
  const std::vector<DescriptorsPerFrame> descriptors_;

  // The pipeline builder is preserved, so that the user may update it without
  // rebuilding the entire model.
  std::unique_ptr<PipelineBuilder> pipeline_builder_;

  // Wrapper of VkPipeline.
  std::unique_ptr<Pipeline> pipeline_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_MODEL_H */
