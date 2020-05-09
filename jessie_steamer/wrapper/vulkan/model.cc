//
//  model.cc
//
//  Created by Pujun Lun on 3/22/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/model.h"

#include "jessie_steamer/common/file.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"
#include "jessie_steamer/wrapper/vulkan/image_util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using common::Vertex3DWithTex;
using VertexInfo = PerVertexBuffer::NoShareIndicesDataInfo;

constexpr uint32_t kPerVertexBufferBindingPoint = 0;
constexpr uint32_t kPerInstanceBufferBindingPointBase = 1;

// Visits variants of TextureSource and constructs a texture from 'source'.
std::unique_ptr<SamplableImage> CreateTexture(
    const SharedBasicContext& context,
    const ModelBuilder::TextureSource& source) {
  if (absl::holds_alternative<SharedTexture::SourcePath>(source)) {
    const auto image_usages = {image::Usage::GetSampledInFragmentShaderUsage()};
    return absl::make_unique<SharedTexture>(
        context, absl::get<SharedTexture::SourcePath>(source),
        image_usages, ImageSampler::Config{});
  } else if (absl::holds_alternative<OffscreenImagePtr>(source)) {
    return absl::make_unique<UnownedOffscreenTexture>(
        absl::get<OffscreenImagePtr>(source));
  } else {
    FATAL("Unrecognized variant type");
  }
}

// Traverses textures and populates 'descriptor_info' and 'image_info_map'.
// All textures of the same type will be bound to the same binding point.
// If there is any texture of a type, that type must exist in 'binding_map'.
void CreateTextureInfo(const ModelBuilder::BindingPointMap& binding_map,
                       const ModelBuilder::TexturesPerMesh& mesh_textures,
                       const ModelBuilder::TexturesPerMesh& shared_textures,
                       Descriptor::Info* descriptor_info,
                       Descriptor::ImageInfoMap* image_info_map) {
  *descriptor_info = Descriptor::Info{
      Image::GetDescriptorTypeForSampling(),
      VK_SHADER_STAGE_FRAGMENT_BIT,
      /*bindings=*/{},  // To be updated.
  };
  auto& texture_bindings = descriptor_info->bindings;

  static constexpr auto kNumTypes =
      static_cast<int>(ModelBuilder::TextureType::kNumTypes);
  for (int type_index = 0; type_index < kNumTypes; ++type_index) {
    const auto texture_type =
        static_cast<ModelBuilder::TextureType>(type_index);
    const int num_textures = mesh_textures[type_index].size() +
                             shared_textures[type_index].size();

    if (num_textures != 0) {
      const auto found = binding_map.find(texture_type);
      ASSERT_TRUE(found != binding_map.end(),
                  absl::StrFormat("Binding point of texture type %d is not set",
                                  type_index));
      const auto binding_point = found->second;

      // Populate 'descriptor_info' with declarations of resources.
      texture_bindings.push_back(Descriptor::Info::Binding{
          binding_point,
          /*array_length=*/static_cast<uint32_t>(num_textures),
      });

      // Populate 'image_info_map' with image descriptor infos.
      auto& descriptor_infos = (*image_info_map)[binding_point];
      descriptor_infos.reserve(num_textures);
      for (const auto& texture : mesh_textures[type_index]) {
        descriptor_infos.push_back(texture->GetDescriptorInfoForSampling());
      }
      for (const auto& texture : shared_textures[type_index]) {
        descriptor_infos.push_back(texture->GetDescriptorInfoForSampling());
      }
    }
  }
}

// Creates push constant ranges given 'push_constant_infos', assuming that
// PushConstant::size_per_frame() bytes will be sent in each frame.
std::vector<VkPushConstantRange> CreatePushConstantRanges(
    const ModelBuilder::PushConstantInfos& push_constant_infos) {
  std::vector<VkPushConstantRange> ranges;
  ranges.reserve(push_constant_infos.infos.size());
  for (const auto& info : push_constant_infos.infos) {
    ranges.push_back(VkPushConstantRange{
        push_constant_infos.shader_stage,
        info.target_offset,
        info.push_constant->size_per_frame(),
    });
  }
  return ranges;
}

// Updates 'pipeline_builder' with vertex input bindings and attributes,
// assuming per-vertex data is of type Vertex3DWithTex.
void SetPipelineVertexInput(
    const PerVertexBuffer& per_vertex_buffer,
    const std::vector<const PerInstanceBuffer*>& per_instance_buffers,
    GraphicsPipelineBuilder* pipeline_builder) {
  uint32_t attribute_start_location = 0;

  auto per_vertex_attributes =
      per_vertex_buffer.GetAttributes(attribute_start_location);
  attribute_start_location += per_vertex_attributes.size();
  pipeline_builder->AddVertexInput(
      kPerVertexBufferBindingPoint,
      pipeline::GetPerVertexBindingDescription<Vertex3DWithTex>(),
      std::move(per_vertex_attributes));

  for (int i = 0; i < per_instance_buffers.size(); ++i) {
    ASSERT_NON_NULL(per_instance_buffers[i],
                    "Per-instance vertex buffer not provided");
    auto per_instance_binding = pipeline::GetBindingDescription(
        /*stride=*/per_instance_buffers[i]->per_instance_data_size(),
        /*instancing=*/true);
    auto per_instance_attributes =
        per_instance_buffers[i]->GetAttributes(attribute_start_location);
    attribute_start_location += per_instance_attributes.size();
    pipeline_builder->AddVertexInput(
        kPerInstanceBufferBindingPointBase + i,
        std::move(per_instance_binding), std::move(per_instance_attributes));
  }
}

} /* namespace */

void ModelBuilder::SingleMeshResource::LoadMesh(ModelBuilder* builder) const {
  // Load indices and vertices.
  const common::ObjFile file{obj_file_path_, obj_file_index_base_};
  VertexInfo vertex_info{
      /*per_mesh_infos=*/{{
          PerVertexBuffer::VertexDataInfo{file.indices},
          PerVertexBuffer::VertexDataInfo{file.vertices},
      }},
  };
  builder->vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
      builder->context_, std::move(vertex_info),
      pipeline::GetVertexAttribute<Vertex3DWithTex>());

  // Load textures.
  auto& mesh_textures = builder->mesh_textures_;
  mesh_textures.push_back({});
  for (const auto& pair : tex_source_map_) {
    const auto type_index = static_cast<int>(pair.first);
    const auto& sources = pair.second;
    mesh_textures.back()[type_index].reserve(sources.size());
    for (const auto& source : sources) {
      mesh_textures.back()[type_index].push_back(
          CreateTexture(builder->context_, source));
    }
  }
}

void ModelBuilder::MultiMeshResource::LoadMesh(ModelBuilder* builder) const {
  // Load indices and vertices.
  const common::ModelLoader loader{model_path_, texture_dir_};
  std::vector<VertexInfo::PerMeshInfo> per_mesh_infos;
  per_mesh_infos.reserve(loader.mesh_datas().size());
  for (const auto& mesh_data : loader.mesh_datas()) {
    per_mesh_infos.push_back(VertexInfo::PerMeshInfo{
        PerVertexBuffer::VertexDataInfo{mesh_data.indices},
        PerVertexBuffer::VertexDataInfo{mesh_data.vertices},
    });
  }
  builder->vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
      builder->context_, VertexInfo{std::move(per_mesh_infos)},
      pipeline::GetVertexAttribute<Vertex3DWithTex>());

  // Load textures.
  const auto image_usages = {image::Usage::GetSampledInFragmentShaderUsage()};
  auto& mesh_textures = builder->mesh_textures_;
  mesh_textures.reserve(loader.mesh_datas().size());
  for (const auto& mesh_data : loader.mesh_datas()) {
    mesh_textures.push_back({});
    for (const auto& texture : mesh_data.textures) {
      const auto type_index = static_cast<int>(texture.texture_type);
      mesh_textures.back()[type_index].push_back(
          absl::make_unique<SharedTexture>(
              builder->context_, texture.path, image_usages,
              ImageSampler::Config{}));
    }
  }
}

ModelBuilder::ModelBuilder(SharedBasicContext context,
                           std::string&& name,
                           int num_frames_in_flight,
                           float viewport_aspect_ratio,
                           const ModelResource& resource)
    : context_{std::move(FATAL_IF_NULL(context))},
      num_frames_in_flight_{num_frames_in_flight},
      viewport_aspect_ratio_{viewport_aspect_ratio},
      uniform_buffer_info_maps_(num_frames_in_flight_),
      pipeline_builder_{absl::make_unique<GraphicsPipelineBuilder>(context_)} {
  pipeline_builder_->SetPipelineName(std::move(name));
  resource.LoadMesh(this);
}

ModelBuilder& ModelBuilder::AddSharedTexture(
    TextureType type, const TextureSource& texture_source) {
  shared_textures_[static_cast<int>(type)].push_back(
      CreateTexture(context_, texture_source));
  return *this;
}

ModelBuilder& ModelBuilder::AddTextureBindingPoint(TextureType type,
                                                   uint32_t binding_point) {
  texture_binding_map_[type] = binding_point;
  return *this;
}

ModelBuilder& ModelBuilder::AddPerInstanceBuffer(
    const PerInstanceBuffer* buffer) {
  per_instance_buffers_.push_back(buffer);
  return *this;
}

ModelBuilder& ModelBuilder::AddUniformBinding(
    VkShaderStageFlags shader_stage,
    std::vector<Descriptor::Info::Binding>&& bindings) {
  uniform_descriptor_infos_.push_back(Descriptor::Info{
      UniformBuffer::GetDescriptorType(),
      shader_stage,
      std::move(bindings),
  });
  return *this;
}

ModelBuilder& ModelBuilder::AddUniformBuffer(
    uint32_t binding_point, const UniformBuffer& uniform_buffer) {
  for (int frame = 0; frame < num_frames_in_flight_; ++frame) {
    // Uniform buffer may have only one chunk if we want the data to keep
    // constant throughout all frames.
    const int uniform_data_chunk_index =
        uniform_buffer.HasSingleChunk() ? 0 : frame;
    uniform_buffer_info_maps_[frame][binding_point].push_back(
        uniform_buffer.GetDescriptorInfo(uniform_data_chunk_index));
  }
  return *this;
}

ModelBuilder& ModelBuilder::SetPushConstantShaderStage(
    VkShaderStageFlags shader_stage) {
  if (!push_constant_infos_.has_value()) {
    push_constant_infos_.emplace();
  }
  push_constant_infos_->shader_stage = shader_stage;
  return *this;
}

ModelBuilder& ModelBuilder::AddPushConstant(const PushConstant* push_constant,
                                            uint32_t target_offset) {
  if (!push_constant_infos_.has_value()) {
    push_constant_infos_.emplace();
  }
  push_constant_infos_.value().infos.push_back(
      PushConstantInfos::Info{push_constant, target_offset});
  return *this;
}

ModelBuilder& ModelBuilder::SetShader(VkShaderStageFlagBits shader_stage,
                                      std::string&& file_path) {
  pipeline_builder_->SetShader(shader_stage, std::move(file_path));
  return *this;
}

std::vector<ModelBuilder::DescriptorsPerFrame>
ModelBuilder::CreateDescriptors() const {
  // For different frames, we get data from different parts of uniform buffers.
  // For different meshes, we bind different textures.
  // Hence, we need a 2D array: descriptors[num_frames][num_meshes].
  std::vector<DescriptorsPerFrame> descriptors(num_frames_in_flight_);
  auto descriptor_infos = uniform_descriptor_infos_;
  // The last element will store the descriptor info of textures.
  descriptor_infos.resize(descriptor_infos.size() + 1);

  for (int frame = 0; frame < num_frames_in_flight_; ++frame) {
    descriptors[frame].reserve(mesh_textures_.size());

    for (const auto& mesh_texture : mesh_textures_) {
      Descriptor::ImageInfoMap image_info_map;
      CreateTextureInfo(texture_binding_map_, mesh_texture, shared_textures_,
                        &descriptor_infos.back(), &image_info_map);

      descriptors[frame].push_back(
          absl::make_unique<StaticDescriptor>(context_, descriptor_infos));
      descriptors[frame].back()->UpdateBufferInfos(
          UniformBuffer::GetDescriptorType(), uniform_buffer_info_maps_[frame]);
      descriptors[frame].back()->UpdateImageInfos(
          Image::GetDescriptorTypeForSampling(), image_info_map);
    }
  }
  return descriptors;
}

std::unique_ptr<Model> ModelBuilder::Build() {
  if (push_constant_infos_.has_value()) {
    ASSERT_NON_EMPTY(push_constant_infos_->infos,
                     "Push constant data source is not set");
  }

  auto descriptors = CreateDescriptors();
  pipeline_builder_->SetPipelineLayout(
      {descriptors[0][0]->layout()},
      push_constant_infos_.has_value()
          ? CreatePushConstantRanges(push_constant_infos_.value())
          : std::vector<VkPushConstantRange>{});
  SetPipelineVertexInput(*vertex_buffer_, per_instance_buffers_,
                         pipeline_builder_.get());

  uniform_descriptor_infos_.clear();
  uniform_buffer_info_maps_.clear();

  return std::unique_ptr<Model>{new Model{
      context_, viewport_aspect_ratio_, std::move(vertex_buffer_),
      std::move(per_instance_buffers_), std::move(push_constant_infos_),
      std::move(shared_textures_), std::move(mesh_textures_),
      std::move(descriptors), std::move(pipeline_builder_)}};
}

void Model::Update(bool is_object_opaque, const VkExtent2D& frame_size,
                   VkSampleCountFlagBits sample_count,
                   const RenderPass& render_pass, uint32_t subpass_index,
                   bool flip_viewport_y) {
  pipeline_ = (*pipeline_builder_)
      .SetDepthTestEnabled(/*enable_test=*/true,
                           /*enable_write=*/is_object_opaque)
      .SetMultisampling(sample_count)
      .SetViewport(pipeline::GetViewport(frame_size, viewport_aspect_ratio_),
                   flip_viewport_y)
      .SetRenderPass(*render_pass, subpass_index)
      .SetColorBlend(
          std::vector<VkPipelineColorBlendAttachmentState>(
              render_pass.num_color_attachments(subpass_index),
              pipeline::GetColorAlphaBlendState(
                  /*enable_blend=*/!is_object_opaque)))
      .Build();
}

void Model::Draw(const VkCommandBuffer& command_buffer,
                 int frame, uint32_t instance_count) const {
  ASSERT_NON_NULL(pipeline_, "Update() must have been called");
  pipeline_->Bind(command_buffer);
  for (int i = 0; i < per_instance_buffers_.size(); ++i) {
    per_instance_buffers_[i]->Bind(
        command_buffer, kPerInstanceBufferBindingPointBase + i, /*offset=*/0);
  }
  if (push_constant_info_.has_value()) {
    for (const auto& info : push_constant_info_->infos) {
      info.push_constant->Flush(
          command_buffer, pipeline_->layout(), frame, info.target_offset,
          push_constant_info_->shader_stage);
    }
  }
  for (int mesh_index = 0; mesh_index < mesh_textures_.size(); ++mesh_index) {
    descriptors_[frame][mesh_index]->Bind(command_buffer, pipeline_->layout(),
                                          pipeline_->binding_point());
    vertex_buffer_->Draw(command_buffer, kPerVertexBufferBindingPoint,
                         mesh_index, instance_count);
  }
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
