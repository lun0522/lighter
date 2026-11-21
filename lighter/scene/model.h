#ifndef LIGHTER_SCENE_MODEL_H
#define LIGHTER_SCENE_MODEL_H

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "lighter/base/util.h"
#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/scene/model_loader.h"

namespace lighter::scene {

class Model;

// High-level builder that loads Wavefront .obj or Assimp multi-mesh models,
// uploads geometry and textures via renderer::ir::Renderer, and prepares
// per-mesh texture bindings for rendering inside Frame Graph passes.
class ModelBuilder {
 public:
  using TextureType = ModelLoader::TextureType;

  // Cubemap face paths stored as directory + 6 face filenames
  // (+X,-X,+Y,-Y,+Z,-Z).
  struct CubemapPath {
    std::string directory;
    std::vector<std::string> files;
  };

  // Textures can be loaded from a 2D image path, 6 cubemap paths, a
  // caller-owned external renderer::ir::Image, or a per-frame Frame Graph
  // ImageHandle.
  using TextureSource =
      std::variant<std::string, CubemapPath, const renderer::ir::Image*,
                   renderer::ir::ImageHandle>;

  using TextureSourceMap =
      std::unordered_map<TextureType, std::vector<TextureSource>,
                         base::util::EnumClassHash>;

  using BindingPointMap =
      std::unordered_map<TextureType, uint32_t, base::util::EnumClassHash>;

  using SamplerMap =
      std::unordered_map<TextureType, renderer::ir::SamplerDescriptor,
                         base::util::EnumClassHash>;

  // Interface for loading mesh geometry and per-mesh texture sources.
  class ModelResource {
   public:
    virtual ~ModelResource() = default;
    virtual void LoadMesh(ModelBuilder* builder) const = 0;
  };

  // Loads a single Wavefront .obj mesh plus explicit texture sources.
  class SingleMeshResource : public ModelResource {
   public:
    SingleMeshResource(std::string obj_file_path, int obj_file_index_base,
                       TextureSourceMap tex_source_map)
        : obj_file_path_{std::move(obj_file_path)},
          obj_file_index_base_{obj_file_index_base},
          tex_source_map_{std::move(tex_source_map)} {}

    void LoadMesh(ModelBuilder* builder) const override;

   private:
    const std::string obj_file_path_;
    const int obj_file_index_base_;
    const TextureSourceMap tex_source_map_;
  };

  // Loads all meshes and referenced textures via Assimp.
  class MultiMeshResource : public ModelResource {
   public:
    MultiMeshResource(std::string model_path, std::string texture_dir)
        : model_path_{std::move(model_path)},
          texture_dir_{std::move(texture_dir)} {}

    void LoadMesh(ModelBuilder* builder) const override;

   private:
    const std::string model_path_;
    const std::string texture_dir_;
  };

  ModelBuilder(const renderer::ir::Renderer& renderer, std::string name,
               float viewport_aspect_ratio, const ModelResource& resource);

  // This class is neither copyable nor movable.
  ModelBuilder(const ModelBuilder&) = delete;
  ModelBuilder& operator=(const ModelBuilder&) = delete;

  // Adds a texture shared across every mesh in this model (e.g. a skybox).
  ModelBuilder& AddSharedTexture(TextureType type, TextureSource source);

  // Associates a texture type with its shader descriptor binding point.
  ModelBuilder& AddTextureBindingPoint(TextureType type, uint32_t binding);

  // Overrides the sampler config used when binding 'type' (defaults to linear
  // repeat for 2D textures and linear clamp-to-edge for cubemaps).
  ModelBuilder& SetTextureSampler(
      TextureType type, const renderer::ir::SamplerDescriptor& sampler);

  // Appends a per-instance vertex buffer (with attribute locations placed
  // starting at 3, right after Vertex3DWithTex). Caller retains ownership.
  ModelBuilder& AddPerInstanceBuffer(
      std::vector<renderer::ir::VertexBufferView::Attribute> attributes,
      size_t stride, const renderer::ir::Buffer* buffer);

  // Sets the shader path for 'shader_stage'.
  ModelBuilder& SetShader(renderer::ir::shader_stage::ShaderStage shader_stage,
                          std::string_view file_path);

  // Uploads geometry and builds the Model.
  std::unique_ptr<Model> Build();

 private:
  // Associates a semantic TextureType with a host image or texture file path.
  struct TextureEntry {
    TextureType type;
    TextureSource source;
  };

  // CPU-side geometry and texture list collected per mesh before GPU upload.
  struct MeshBuildData {
    std::vector<base::Vertex3DWithTex> vertices;
    std::vector<uint32_t> indices;
    std::vector<TextureEntry> textures;
  };

  void AddLoadedMesh(MeshBuildData&& mesh_data);

  const renderer::ir::Renderer& renderer_;
  const std::string name_;
  const float viewport_aspect_ratio_;

  std::vector<MeshBuildData> mesh_datas_;
  std::vector<TextureEntry> shared_textures_;
  BindingPointMap texture_binding_map_;
  SamplerMap sampler_map_;

  std::vector<const renderer::ir::Buffer*> per_instance_buffers_;
  renderer::ir::GraphicsPipelineDescriptor base_pipeline_descriptor_;
  int next_vertex_attrib_location_ = 3;
  int next_vertex_binding_point_ = 1;
};

// Renders multi-mesh 3D models loaded by ModelBuilder inside Frame Graph
// graphics passes via renderer::ir::PassContext.
class Model {
 public:
  // This class is neither copyable nor movable.
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  // Builds a GraphicsPipelineDescriptor configured for 'frame_size', blending,
  // depth write, and 'num_color_attachments'.
  renderer::ir::GraphicsPipelineDescriptor BuildPipelineDescriptor(
      const glm::ivec2& frame_size, bool is_object_opaque = true,
      bool flip_viewport_y = true, int num_color_attachments = 1) const;

  // Binds the model's pipeline (must be called before pushing per-draw uniforms
  // or push constants onto 'ctx' and calling Draw()).
  void BindPipeline(renderer::ir::PassContext& ctx,
                    const glm::ivec2& frame_size, bool is_object_opaque = true,
                    bool flip_viewport_y = true,
                    int num_color_attachments = 1) const;

  // Binds vertex/index/instance buffers, iterates meshes binding per-mesh
  // textures, and issues DrawIndexed calls.
  void Draw(renderer::ir::PassContext& ctx, uint32_t instance_count = 1) const;

 private:
  friend std::unique_ptr<Model> ModelBuilder::Build();

  // Resolved shader binding target and sampler metadata for one mesh texture.
  struct TextureBinding {
    uint32_t binding = 0;
    uint32_t array_element = 0;
    renderer::ir::SamplerDescriptor sampler{};
    std::variant<const renderer::ir::Image*, renderer::ir::ImageHandle> target;
  };

  // Buffer byte offsets, index count, and texture bindings for one draw call.
  struct MeshDrawInfo {
    size_t vertex_offset = 0;
    size_t index_offset = 0;
    uint32_t index_count = 0;
    std::vector<TextureBinding> texture_bindings;
  };

  Model(float viewport_aspect_ratio,
        renderer::ir::GraphicsPipelineDescriptor base_pipeline_descriptor,
        std::unique_ptr<renderer::ir::Buffer> geometry_buffer,
        std::vector<const renderer::ir::Buffer*> per_instance_buffers,
        std::vector<std::unique_ptr<renderer::ir::Image>> owned_textures,
        std::vector<MeshDrawInfo> meshes)
      : viewport_aspect_ratio_{viewport_aspect_ratio},
        base_pipeline_descriptor_{std::move(base_pipeline_descriptor)},
        geometry_buffer_{std::move(geometry_buffer)},
        per_instance_buffers_{std::move(per_instance_buffers)},
        owned_textures_{std::move(owned_textures)},
        meshes_{std::move(meshes)} {}

  const float viewport_aspect_ratio_;
  const renderer::ir::GraphicsPipelineDescriptor base_pipeline_descriptor_;
  const std::unique_ptr<renderer::ir::Buffer> geometry_buffer_;
  const std::vector<const renderer::ir::Buffer*> per_instance_buffers_;
  const std::vector<std::unique_ptr<renderer::ir::Image>> owned_textures_;
  const std::vector<MeshDrawInfo> meshes_;
};

}  // namespace lighter::scene

#endif  // LIGHTER_SCENE_MODEL_H
