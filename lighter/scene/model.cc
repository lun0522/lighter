#include "lighter/scene/model.h"

#include <array>
#include <format>
#include <utility>

#include "lighter/base/file.h"
#include "lighter/base/image.h"
#include "lighter/renderer/ir/buffer_util.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/pipeline_util.h"

namespace lighter::scene {
namespace {

using base::Vertex3DWithTex;
using renderer::ir::Buffer;
using renderer::ir::BufferUsage;
using renderer::ir::Image;
using renderer::ir::ImageUsage;
using renderer::ir::SamplerDescriptor;

constexpr int kVertexBufferBindingPoint = 0;

SamplerDescriptor GetDefaultSampler(ModelBuilder::TextureType type) {
  if (type == ModelBuilder::TextureType::kCubemap) {
    return SamplerDescriptor{
        .filter_type = renderer::ir::FilterType::kLinear,
        .address_mode = renderer::ir::SamplerAddressMode::kClampToEdge,
    };
  }
  return SamplerDescriptor{
      .filter_type = renderer::ir::FilterType::kLinear,
      .address_mode = renderer::ir::SamplerAddressMode::kRepeat,
  };
}

}  // namespace

void ModelBuilder::SingleMeshResource::LoadMesh(ModelBuilder* builder) const {
  const base::ObjFile file{obj_file_path_, obj_file_index_base_};
  MeshBuildData mesh_data{
      .vertices = file.vertices,
      .indices = file.indices,
  };
  for (const auto& [type, sources] : tex_source_map_) {
    for (const auto& source : sources) {
      mesh_data.textures.push_back({type, source});
    }
  }
  builder->AddLoadedMesh(std::move(mesh_data));
}

void ModelBuilder::MultiMeshResource::LoadMesh(ModelBuilder* builder) const {
  const ModelLoader loader{model_path_, texture_dir_};
  for (const auto& mesh : loader.mesh_datas()) {
    MeshBuildData mesh_data{
        .vertices = mesh.vertices,
        .indices = mesh.indices,
    };
    mesh_data.textures.reserve(mesh.textures.size());
    for (const auto& tex : mesh.textures) {
      mesh_data.textures.push_back({tex.texture_type, tex.path});
    }
    builder->AddLoadedMesh(std::move(mesh_data));
  }
}

ModelBuilder::ModelBuilder(const renderer::ir::Renderer& renderer,
                           std::string name, float viewport_aspect_ratio,
                           const ModelResource& resource)
    : renderer_{renderer},
      name_{std::move(name)},
      viewport_aspect_ratio_{viewport_aspect_ratio} {
  base_pipeline_descriptor_.SetName(name_);
  base_pipeline_descriptor_.AddVertexInput({
      .input_rate = renderer::ir::VertexInputRate::kVertex,
      .binding_point = kVertexBufferBindingPoint,
      .stride = sizeof(Vertex3DWithTex),
      .attributes = renderer::ir::buffer::CreateAttributesForVertex3DWithTex(
          /*loc_pos=*/0, /*loc_norm=*/1, /*loc_tex_coord=*/2),
  });
  resource.LoadMesh(this);
}

void ModelBuilder::AddLoadedMesh(MeshBuildData&& mesh_data) {
  mesh_datas_.push_back(std::move(mesh_data));
}

ModelBuilder& ModelBuilder::AddSharedTexture(TextureType type,
                                             TextureSource source) {
  shared_textures_.push_back({type, std::move(source)});
  return *this;
}

ModelBuilder& ModelBuilder::AddTextureBindingPoint(TextureType type,
                                                   uint32_t binding) {
  texture_binding_map_[type] = binding;
  return *this;
}

ModelBuilder& ModelBuilder::SetTextureSampler(
    TextureType type, const renderer::ir::SamplerDescriptor& sampler) {
  sampler_map_[type] = sampler;
  return *this;
}

ModelBuilder& ModelBuilder::AddPerInstanceBuffer(
    std::vector<renderer::ir::VertexBufferView::Attribute> attributes,
    size_t stride, const renderer::ir::Buffer* buffer) {
  ASSERT_NON_NULL(buffer, "Per-instance buffer must not be nullptr");
  for (auto& attrib : attributes) {
    attrib.location += next_vertex_attrib_location_;
  }
  next_vertex_attrib_location_ += static_cast<int>(attributes.size());
  base_pipeline_descriptor_.AddVertexInput({
      .input_rate = renderer::ir::VertexInputRate::kInstance,
      .binding_point = next_vertex_binding_point_++,
      .stride = stride,
      .attributes = std::move(attributes),
  });
  per_instance_buffers_.push_back(buffer);
  return *this;
}

ModelBuilder& ModelBuilder::SetShader(
    renderer::ir::shader_stage::ShaderStage shader_stage,
    std::string_view file_path) {
  base_pipeline_descriptor_.SetShader(shader_stage, file_path);
  return *this;
}

std::unique_ptr<Model> ModelBuilder::Build() {
  ASSERT_NON_EMPTY(mesh_datas_, "Model has no meshes");

  // Compute total packed buffer size and record copy slices (vertices followed
  // by uint32 indices; sizeof(Vertex3DWithTex) is a multiple of 4 so offsets
  // remain 4-byte aligned).
  size_t total_size = 0;
  std::vector<Buffer::CopyInfo> copy_infos;
  copy_infos.reserve(mesh_datas_.size() * 2);
  std::vector<std::pair<size_t, size_t>> mesh_offsets;
  mesh_offsets.reserve(mesh_datas_.size());

  for (const auto& mesh : mesh_datas_) {
    const size_t vertex_offset = total_size;
    const size_t vertex_bytes = base::util::GetTotalDataSize(
        std::span<const Vertex3DWithTex>{mesh.vertices});
    if (vertex_bytes > 0) {
      copy_infos.push_back({mesh.vertices.data(), vertex_bytes, vertex_offset});
    }
    total_size += vertex_bytes;

    const size_t index_offset = total_size;
    const size_t index_bytes =
        base::util::GetTotalDataSize(std::span<const uint32_t>{mesh.indices});
    if (index_bytes > 0) {
      copy_infos.push_back({mesh.indices.data(), index_bytes, index_offset});
    }
    total_size += index_bytes;
    mesh_offsets.emplace_back(vertex_offset, index_offset);
  }

  auto geometry_buffer =
      renderer_.CreateBuffer(Buffer::UpdateRate::kLow, total_size,
                             std::array{BufferUsage::GetVertexBufferUsage(
                                 BufferUsage::UsageType::kIndexAndVertex)});
  geometry_buffer->CopyToDevice(copy_infos);

  // Deduplicate textures loaded from identical file paths.
  std::vector<std::unique_ptr<Image>> owned_textures;
  std::unordered_map<std::string, const Image*> path_cache;
  const std::array sampled_usage{ImageUsage::GetSampledInFragmentShaderUsage()};

  const auto resolve_target = [&](const TextureSource& source)
      -> std::variant<const Image*, renderer::ir::ImageHandle> {
    if (const auto* path = std::get_if<std::string>(&source)) {
      const auto [iter, inserted] = path_cache.try_emplace(*path, nullptr);
      if (inserted) {
        const base::Image loaded =
            base::Image::LoadSingleImageFromFile(*path, /*flip_y=*/true);
        owned_textures.push_back(renderer_.CreateColorImage(
            *path, loaded, /*generate_mipmaps=*/true, sampled_usage));
        iter->second = owned_textures.back().get();
      }
      return iter->second;
    }
    if (const auto* cubemap = std::get_if<CubemapPath>(&source)) {
      std::string cache_key = cubemap->directory;
      for (const std::string& file : cubemap->files) {
        cache_key += '|';
        cache_key += file;
      }
      const auto [iter, inserted] = path_cache.try_emplace(cache_key, nullptr);
      if (inserted) {
        const base::Image loaded = base::Image::LoadCubemapFromFiles(
            cubemap->directory, cubemap->files, /*flip_y=*/false);
        owned_textures.push_back(renderer_.CreateColorImage(
            cache_key, loaded, /*generate_mipmaps=*/false, sampled_usage));
        iter->second = owned_textures.back().get();
      }
      return iter->second;
    }
    if (const auto* external = std::get_if<const Image*>(&source)) {
      ASSERT_NON_NULL(*external, "External texture image must not be nullptr");
      return *external;
    }
    return std::get<renderer::ir::ImageHandle>(source);
  };

  std::vector<Model::MeshDrawInfo> meshes;
  meshes.reserve(mesh_datas_.size());
  for (size_t i = 0; i < mesh_datas_.size(); ++i) {
    const auto& mesh_data = mesh_datas_[i];
    std::array<uint32_t, static_cast<size_t>(TextureType::kNumTypes)>
        type_counts{};
    std::vector<Model::TextureBinding> bindings;

    const auto append_texture = [&](const TextureEntry& entry) {
      const auto binding_iter = texture_binding_map_.find(entry.type);
      ASSERT_TRUE(binding_iter != texture_binding_map_.end(),
                  std::format("Binding point for texture type {} is not set",
                              static_cast<int>(entry.type)));
      const auto sampler_iter = sampler_map_.find(entry.type);
      const SamplerDescriptor sampler = sampler_iter != sampler_map_.end()
                                            ? sampler_iter->second
                                            : GetDefaultSampler(entry.type);
      const size_t type_idx = static_cast<size_t>(entry.type);
      bindings.push_back(Model::TextureBinding{
          .binding = binding_iter->second,
          .array_element = type_counts[type_idx]++,
          .sampler = sampler,
          .target = resolve_target(entry.source),
      });
    };

    for (const auto& entry : mesh_data.textures) {
      append_texture(entry);
    }
    for (const auto& entry : shared_textures_) {
      append_texture(entry);
    }

    meshes.push_back(Model::MeshDrawInfo{
        .vertex_offset = mesh_offsets[i].first,
        .index_offset = mesh_offsets[i].second,
        .index_count = static_cast<uint32_t>(mesh_data.indices.size()),
        .texture_bindings = std::move(bindings),
    });
  }

  return std::unique_ptr<Model>{
      new Model{viewport_aspect_ratio_, std::move(base_pipeline_descriptor_),
                std::move(geometry_buffer), std::move(per_instance_buffers_),
                std::move(owned_textures), std::move(meshes)}};
}

renderer::ir::GraphicsPipelineDescriptor Model::BuildPipelineDescriptor(
    const glm::ivec2& frame_size, bool is_object_opaque, bool flip_viewport_y,
    int num_color_attachments) const {
  renderer::ir::GraphicsPipelineDescriptor desc = base_pipeline_descriptor_;
  if (is_object_opaque) {
    desc.EnableDepthTestAndWrite();
  } else {
    desc.EnableDepthTestOnly();
  }
  const std::optional<renderer::ir::GraphicsPipelineDescriptor::ColorBlend>
      blend = is_object_opaque
                  ? std::nullopt
                  : std::optional{renderer::ir::pipeline::GetColorAlphaBlend()};
  for (int i = 0; i < num_color_attachments; ++i) {
    desc.UseColorAttachment(i, blend);
  }
  desc.SetViewport(
      renderer::ir::pipeline::GetViewport(frame_size, viewport_aspect_ratio_),
      renderer::ir::pipeline::GetFullFrameScissor(frame_size), flip_viewport_y);
  return desc;
}

void Model::BindPipeline(renderer::ir::PassContext& ctx,
                         const glm::ivec2& frame_size, bool is_object_opaque,
                         bool flip_viewport_y,
                         int num_color_attachments) const {
  ctx.BindGraphicsPipeline(BuildPipelineDescriptor(
      frame_size, is_object_opaque, flip_viewport_y, num_color_attachments));
}

void Model::Draw(renderer::ir::PassContext& ctx,
                 uint32_t instance_count) const {
  for (size_t i = 0; i < per_instance_buffers_.size(); ++i) {
    ctx.BindVertexBuffer(
        static_cast<uint32_t>(kVertexBufferBindingPoint + 1 + i),
        *per_instance_buffers_[i]);
  }
  for (const auto& mesh : meshes_) {
    ctx.BindVertexBuffer(kVertexBufferBindingPoint, *geometry_buffer_,
                         mesh.vertex_offset);
    ctx.BindIndexBuffer(*geometry_buffer_, mesh.index_offset,
                        /*is_uint16=*/false);
    for (const auto& tex : mesh.texture_bindings) {
      if (const auto* external = std::get_if<const Image*>(&tex.target)) {
        ctx.BindExternalImage(tex.binding, **external, &tex.sampler,
                              tex.array_element);
      } else {
        ctx.BindImage(tex.binding,
                      std::get<renderer::ir::ImageHandle>(tex.target),
                      &tex.sampler, tex.array_element);
      }
    }
    ctx.DrawIndexed(mesh.index_count, instance_count);
  }
}

}  // namespace lighter::scene
