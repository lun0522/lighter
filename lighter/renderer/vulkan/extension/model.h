//
//  model.h
//
//  Created by Pujun Lun on 3/22/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_MODEL_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_MODEL_H

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "lighter/common/model_loader.h"
#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/buffer.h"
#include "lighter/renderer/vulkan/wrapper/descriptor.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "lighter/renderer/vulkan/wrapper/pipeline.h"
#include "lighter/renderer/vulkan/wrapper/pipeline_util.h"
#include "lighter/renderer/vulkan/wrapper/render_pass.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/variant.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {

// Forward declarations.
class Model;

// The user should use this class to create Model. After calling Build(),
// internal states will be invalidated, and the builder should be discarded.
// When building multiple models that share shaders, the user may use
// AutoReleaseShaderPool to prevent shaders from being auto released.
class ModelBuilder {
 public:
  // An instance of this will preserve all shader modules created within its
  // scope, and release them once goes out of scope.
  using AutoReleaseShaderPool = ShaderModule::AutoReleaseShaderPool;

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

  // Interface of model resource classes.
  class ModelResource {
   public:
    virtual ~ModelResource() = default;

    // Loads meshes and textures, and populates 'vertex_buffer_' and
    // 'mesh_textures_' of 'builder'.
    virtual void LoadMesh(ModelBuilder* builder) const = 0;
  };

  // Contains information required for loading one mesh from the Wavefront .obj
  // file at 'obj_file_path' and textures in 'tex_source_map' using a
  // lightweight .obj file loader.
  class SingleMeshResource : public ModelResource {
   public:
    SingleMeshResource(std::string&& obj_file_path,
                       int obj_file_index_base,
                       TextureSourceMap&& tex_source_map)
        : obj_file_path_{std::move(obj_file_path)},
          obj_file_index_base_{obj_file_index_base},
          tex_source_map_{std::move(tex_source_map)} {}

    // Overrides.
    void LoadMesh(ModelBuilder* builder) const override;

   private:
    const std::string obj_file_path_;
    const int obj_file_index_base_;
    const TextureSourceMap tex_source_map_;
  };

  // Contains information required for loading the model from 'model_path' and
  // textures from 'texture_dir' using Assimp.
  class MultiMeshResource : public ModelResource {
   public:
    MultiMeshResource(std::string&& model_path, std::string&& texture_dir)
        : model_path_{std::move(model_path)},
          texture_dir_{std::move(texture_dir)} {}

    // Overrides.
    void LoadMesh(ModelBuilder* builder) const override;

   private:
    const std::string model_path_;
    const std::string texture_dir_;
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

  // When the frame is resized, the aspect ratio of viewport will always be
  // 'viewport_aspect_ratio'. If any offscreen images are used in 'resource',
  // the user is responsible for keeping the existence of them.
  ModelBuilder(SharedBasicContext context,
               std::string&& name,
               int num_frames_in_flight,
               float viewport_aspect_ratio,
               const ModelResource& resource);

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
  // Note that this class assumes the per-vertex data type is Vertex3DWithTex,
  // hence vertex attributes of user-provided per-instance buffers will be bound
  // to locations starting from 3.
  ModelBuilder& AddPerInstanceBuffer(const PerInstanceBuffer* buffer);

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

  // Loads a shader that will be used at 'shader_stage' from 'file_path'.
  ModelBuilder& SetShader(VkShaderStageFlagBits shader_stage,
                          std::string&& file_path);

  // Returns a model. All internal states will be invalidated after this.
  // The user should discard the builder, and perform future updates on the
  // returned model directly.
  std::unique_ptr<Model> Build();

 private:
  // Creates descriptors for all resources used for rendering the model.
  std::vector<DescriptorsPerFrame> CreateDescriptors() const;

  // Pointer to context.
  const SharedBasicContext context_;

  // Number of frames in flight.
  const int num_frames_in_flight_;

  // Aspect ratio of the viewport. This is used to make sure the aspect ratio of
  // the object does not change when the size of framebuffers changes.
  const float viewport_aspect_ratio_;

  // Holds per-vertex data.
  std::unique_ptr<StaticPerVertexBuffer> vertex_buffer_;

  // Each element stores textures used for the mesh at the same index.
  std::vector<TexturesPerMesh> mesh_textures_;

  // Textures shared by all meshes.
  TexturesPerMesh shared_textures_;

  // Maps each texture type to its binding point.
  BindingPointMap texture_binding_map_;

  // Per-instance vertex buffers.
  std::vector<const PerInstanceBuffer*> per_instance_buffers_;

  // Declares uniform data used in shaders.
  std::vector<Descriptor::Info> uniform_descriptor_infos_;

  // Each element maps binding points to buffer infos of the uniform buffers
  // bound to them. Elements are indexed by the frame, and the length of this
  // should be equal to 'num_frames_in_flight_'.
  std::vector<Descriptor::BufferInfoMap> uniform_buffer_info_maps_;

  // Describes push constant data sources.
  absl::optional<PushConstantInfos> push_constant_infos_;

  // Builder of graphics pipeline.
  std::unique_ptr<GraphicsPipelineBuilder> pipeline_builder_;
};

// The Model and its builder class are used to:
//   - Load and bind per-vertex data and textures.
//   - Bind vertex buffers (used for instancing), uniform buffers and push
//     constants.
//   - Load shaders of all stages.
//   - Maintain a graphics pipeline internally, and render the model during
//     command buffer recordings.
// The user should use ModelBuilder to create instances of this class.
// Update() must have been called before calling Draw() for the first time, and
// whenever the render pass is changed, or if the user wants to change the
// transparency of the object.
class Model {
 public:
  // This class is neither copyable nor movable.
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  // TODO: Transparency and viewport can be changed via dynamic states.
  // Updates internal states and rebuilds the graphics pipeline.
  // For simplicity, the render area will be the same to 'frame_size'.
  // If 'flip_viewport_y' is true, point (0, 0) will be located at the upper
  // left corner, which is appropriate for presenting to the screen. The user
  // can choose whether or not to do the flipping for offscreen rendering.
  // TODO: Pass VkRendrPass instead.
  void Update(bool is_object_opaque,
              const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
              const RenderPass& render_pass, uint32_t subpass_index,
              bool flip_viewport_y = true);

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
        float viewport_aspect_ratio,
        std::unique_ptr<StaticPerVertexBuffer>&& vertex_buffer,
        std::vector<const PerInstanceBuffer*>&& per_instance_buffers,
        absl::optional<PushConstantInfos>&& push_constant_info,
        TexturesPerMesh&& shared_textures,
        std::vector<TexturesPerMesh>&& mesh_textures,
        std::vector<DescriptorsPerFrame>&& descriptors,
        std::unique_ptr<GraphicsPipelineBuilder>&& pipeline_builder)
      : context_{std::move(FATAL_IF_NULL(context))},
        viewport_aspect_ratio_{viewport_aspect_ratio},
        vertex_buffer_{std::move(vertex_buffer)},
        per_instance_buffers_{std::move(per_instance_buffers)},
        push_constant_info_{std::move(push_constant_info)},
        shared_textures_{std::move(shared_textures)},
        mesh_textures_{std::move(mesh_textures)},
        descriptors_{std::move(descriptors)},
        pipeline_builder_{std::move(pipeline_builder)} {}

  // Pointer to context.
  const SharedBasicContext context_;

  // Aspect ratio of the viewport. This is used to make sure the aspect ratio of
  // the object does not change when the size of framebuffers changes.
  const float viewport_aspect_ratio_;

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
  std::unique_ptr<GraphicsPipelineBuilder> pipeline_builder_;

  // Wrapper of VkPipeline.
  std::unique_ptr<Pipeline> pipeline_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_MODEL_H */
