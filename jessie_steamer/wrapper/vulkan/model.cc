//
//  model.cc
//
//  Created by Pujun Lun on 3/22/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/model.h"

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using common::Vertex3DWithTex;
using std::vector;

using VertexInfo = PerVertexBuffer::NoShareIndicesDataInfo;

// Visits variants of TextureSource and constructs a texture from 'source'.
std::unique_ptr<SamplableImage> CreateTexture(
    const SharedBasicContext& context,
    const ModelBuilder::TextureSource& source) {
  if (absl::holds_alternative<SharedTexture::SourcePath>(source)) {
    return absl::make_unique<SharedTexture>(
        context, absl::get<SharedTexture::SourcePath>(source));
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
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
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
      texture_bindings.emplace_back(Descriptor::Info::Binding{
          binding_point,
          /*array_length=*/static_cast<uint32_t>(num_textures),
      });

      // Populate 'image_info_map' with image descriptor infos.
      auto& descriptor_infos = (*image_info_map)[binding_point];
      descriptor_infos.reserve(num_textures);
      for (const auto& texture : mesh_textures[type_index]) {
        descriptor_infos.emplace_back(texture->GetDescriptorInfo());
      }
      for (const auto& texture : shared_textures[type_index]) {
        descriptor_infos.emplace_back(texture->GetDescriptorInfo());
      }
    }
  }
}

// Creates push constant ranges given 'push_constant_infos', assuming that
// PushConstant::size_per_frame() bytes will be sent in each frame.
vector<VkPushConstantRange> CreatePushConstantRanges(
    const ModelBuilder::PushConstantInfos& push_constant_infos) {
  vector<VkPushConstantRange> ranges;
  ranges.reserve(push_constant_infos.infos.size());
  for (const auto& info : push_constant_infos.infos) {
    ranges.emplace_back(VkPushConstantRange{
        push_constant_infos.shader_stage,
        info.target_offset,
        info.push_constant->size_per_frame(),
    });
  }
  return ranges;
}

// Extracts per-instance vertex buffer infos, updates 'pipeline_builder' with
// vertex input bindings and attributes, and populates 'per_instance_buffers'.
void ExtractPerInstanceBufferInfos(
    const vector<ModelBuilder::PerInstanceBufferInfo>& infos,
    PipelineBuilder* pipeline_builder,
    vector<const PerInstanceBuffer*>* per_instance_buffers) {
  per_instance_buffers->reserve(infos.size());

  vector<pipeline::VertexInputBinding> bindings;
  bindings.reserve(infos.size() + 1);
  bindings.emplace_back(pipeline::GetPerVertexBinding<Vertex3DWithTex>());

  vector<pipeline::VertexInputAttribute> attributes;
  attributes.reserve(infos.size() + 1);
  attributes.emplace_back(pipeline::GetPerVertexAttribute<Vertex3DWithTex>());

  for (int i = 0; i < infos.size(); ++i) {
    const auto& info = infos[i];
    ASSERT_NON_NULL(info.per_instance_buffer,
                    "Per-instance vertex buffer not provided");
    per_instance_buffers->emplace_back(info.per_instance_buffer);
    bindings.emplace_back(pipeline::VertexInputBinding{
        kPerInstanceBindingPointBase + i,
        info.per_instance_data_size,
        /*instancing=*/true,
    });
    attributes.emplace_back(pipeline::VertexInputAttribute{
        kPerInstanceBindingPointBase + i,
        info.vertex_input_attributes,
    });
  }

  pipeline_builder->SetVertexInput(
      pipeline::GetBindingDescriptions(bindings),
      pipeline::GetAttributeDescriptions(attributes));
}

} /* namespace */

ModelBuilder::ModelBuilder(SharedBasicContext context,
                           int num_frames_in_flight,
                           const ModelResource& resource)
    : context_{std::move(context)},
      num_frames_in_flight_{num_frames_in_flight},
      uniform_buffer_info_maps_(num_frames_in_flight_) {
  if (absl::holds_alternative<SingleMeshResource>(resource)) {
    LoadSingleMesh(absl::get<SingleMeshResource>(resource));
  } else if (absl::holds_alternative<MultiMeshResource>(resource)) {
    LoadMultiMesh(absl::get<MultiMeshResource>(resource));
  } else {
    FATAL("Unrecognized variant type");
  }
}

void ModelBuilder::LoadSingleMesh(const SingleMeshResource& resource) {
  // Load indices and vertices.
  const common::ObjFile file{resource.obj_path, resource.obj_file_index_base};
  vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
      context_, VertexInfo{
          /*per_mesh_infos=*/{{
              PerVertexBuffer::VertexDataInfo{file.indices},
              PerVertexBuffer::VertexDataInfo{file.vertices},
          }},
      });

  // Load textures.
  mesh_textures_.emplace_back();
  for (const auto& pair : resource.tex_source_map) {
    const auto type_index = static_cast<int>(pair.first);
    const auto& sources = pair.second;
    mesh_textures_.back()[type_index].reserve(sources.size());
    for (const auto& source : sources) {
      mesh_textures_.back()[type_index].emplace_back(
          CreateTexture(context_, source));
    }
  }
}

void ModelBuilder::LoadMultiMesh(const MultiMeshResource& resource) {
  // Load indices and vertices.
  const common::ModelLoader loader{resource.model_path, resource.texture_dir};
  VertexInfo vertex_info;
  vertex_info.per_mesh_infos.reserve(loader.mesh_datas().size());
  for (const auto& mesh_data : loader.mesh_datas()) {
    vertex_info.per_mesh_infos.emplace_back(VertexInfo::PerMeshInfo{
        PerVertexBuffer::VertexDataInfo{mesh_data.indices},
        PerVertexBuffer::VertexDataInfo{mesh_data.vertices},
    });
  }
  vertex_buffer_ =
      absl::make_unique<StaticPerVertexBuffer>(context_, vertex_info);

  // Load textures.
  mesh_textures_.reserve(loader.mesh_datas().size());
  for (const auto& mesh_data : loader.mesh_datas()) {
    mesh_textures_.emplace_back();
    for (const auto& texture : mesh_data.textures) {
      const auto type_index = static_cast<int>(texture.texture_type);
      mesh_textures_.back()[type_index].emplace_back(
          absl::make_unique<SharedTexture>(context_, texture.path));
    }
  }
}

ModelBuilder& ModelBuilder::AddSharedTexture(
    TextureType type, const TextureSource& texture_source) {
  shared_textures_[static_cast<int>(type)].emplace_back(
      CreateTexture(context_, texture_source));
  return *this;
}

ModelBuilder& ModelBuilder::AddTextureBindingPoint(TextureType type,
                                                   uint32_t binding_point) {
  texture_binding_map_[type] = binding_point;
  return *this;
}

ModelBuilder& ModelBuilder::AddPerInstanceBuffer(PerInstanceBufferInfo&& info) {
  per_instance_buffer_infos_.emplace_back(std::move(info));
  return *this;
}

ModelBuilder& ModelBuilder::AddUniformBinding(
    VkShaderStageFlags shader_stage,
    vector<Descriptor::Info::Binding>&& bindings) {
  uniform_descriptor_infos_.emplace_back(Descriptor::Info{
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      shader_stage,
      std::move(bindings),
  });
  return *this;
}

ModelBuilder& ModelBuilder::AddUniformBuffer(
    uint32_t binding_point, const UniformBuffer& uniform_buffer) {
  for (int frame = 0; frame < num_frames_in_flight_; ++frame) {
    uniform_buffer_info_maps_[frame][binding_point].emplace_back(
        uniform_buffer.GetDescriptorInfo(frame));
  }
  return *this;
}

ModelBuilder& ModelBuilder::SetPushConstantShaderStage(
    VkShaderStageFlags shader_stage) {
  if (!push_constant_infos_.has_value()) {
    push_constant_infos_.emplace();
  }
  push_constant_infos_.value().shader_stage = shader_stage;
  return *this;
}

ModelBuilder& ModelBuilder::AddPushConstant(const PushConstant* push_constant,
                                            uint32_t target_offset) {
  if (!push_constant_infos_.has_value()) {
    push_constant_infos_.emplace();
  }
  push_constant_infos_.value().infos.emplace_back(
      PushConstantInfos::Info{push_constant, target_offset});
  return *this;
}

ModelBuilder& ModelBuilder::AddShader(VkShaderStageFlagBits shader_stage,
                                      std::string&& file_path) {
  shader_infos_.emplace_back(shader_stage, std::move(file_path));
  return *this;
}

vector<ModelBuilder::DescriptorsPerFrame>
ModelBuilder::CreateDescriptors() const {
  // For different frames, we get data from different parts of uniform buffers.
  // For different meshes, we bind different textures.
  // Hence, we need a 2D array: descriptors[num_frames][num_meshes].
  vector<DescriptorsPerFrame> descriptors(num_frames_in_flight_);
  auto descriptor_infos = uniform_descriptor_infos_;
  // The last element will store the descriptor info of textures.
  descriptor_infos.resize(descriptor_infos.size() + 1);

  for (int frame = 0; frame < num_frames_in_flight_; ++frame) {
    descriptors[frame].reserve(mesh_textures_.size());

    for (const auto& mesh_texture : mesh_textures_) {
      Descriptor::ImageInfoMap image_info_map;
      CreateTextureInfo(texture_binding_map_, mesh_texture, shared_textures_,
                        &descriptor_infos.back(), &image_info_map);

      descriptors[frame].emplace_back(
          absl::make_unique<StaticDescriptor>(context_, descriptor_infos));
      // TODO: Derive descriptor type.
      descriptors[frame].back()->UpdateBufferInfos(
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniform_buffer_info_maps_[frame]);
      descriptors[frame].back()->UpdateImageInfos(
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info_map);
    }
  }
  return descriptors;
}

std::unique_ptr<Model> ModelBuilder::Build() {
  if (push_constant_infos_.has_value()) {
    ASSERT_NON_EMPTY(push_constant_infos_.value().infos,
                     "Push constant data source is not set");
  }
  ASSERT_NON_EMPTY(shader_infos_, "Shader is not set");

  auto pipeline_builder = absl::make_unique<PipelineBuilder>(context_);
  auto descriptors = CreateDescriptors();
  pipeline_builder->SetPipelineLayout(
      {descriptors[0][0]->layout()},
      push_constant_infos_.has_value()
          ? CreatePushConstantRanges(push_constant_infos_.value())
          : vector<VkPushConstantRange>{});

  vector<const PerInstanceBuffer*> per_instance_buffers;
  ExtractPerInstanceBufferInfos(per_instance_buffer_infos_,
                                pipeline_builder.get(), &per_instance_buffers);

  per_instance_buffer_infos_.clear();
  uniform_descriptor_infos_.clear();
  uniform_buffer_info_maps_.clear();

  return std::unique_ptr<Model>{new Model{
      context_, std::move(shader_infos_), std::move(vertex_buffer_),
      std::move(per_instance_buffers), std::move(push_constant_infos_),
      std::move(shared_textures_), std::move(mesh_textures_),
      std::move(descriptors), std::move(pipeline_builder)}};
}

void Model::Update(bool is_object_opaque, const VkExtent2D& frame_size,
                   VkSampleCountFlagBits sample_count,
                   const RenderPass& render_pass, uint32_t subpass_index) {
  (*pipeline_builder_)
      .SetDepthTestEnabled(/*enable_test=*/is_object_opaque)
      .SetMultisampling(sample_count)
      .SetViewport(
          /*viewport=*/VkViewport{
              /*x=*/0.0f,
              /*y=*/0.0f,
              static_cast<float>(frame_size.width),
              static_cast<float>(frame_size.height),
              /*minDepth=*/0.0f,
              /*maxDepth=*/1.0f,
          },
          /*scissor=*/VkRect2D{
              /*offset=*/{0, 0},
              frame_size,
          }
      )
      .SetRenderPass(*render_pass, subpass_index)
      .SetColorBlend(
          vector<VkPipelineColorBlendAttachmentState>(
              render_pass.num_color_attachments(subpass_index),
              pipeline::GetColorBlendState(
                  /*enable_blend=*/!is_object_opaque)));
  for (const auto& info : shader_infos_) {
    pipeline_builder_->AddShader(/*shader_stage=*/info.first,
                                 /*file_path=*/info.second);
  }
  pipeline_ = pipeline_builder_->Build();
}

void Model::Draw(const VkCommandBuffer& command_buffer,
                 int frame, uint32_t instance_count) const {
  pipeline_->Bind(command_buffer);
  for (int i = 0; i < per_instance_buffers_.size(); ++i) {
    per_instance_buffers_[i]->Bind(command_buffer,
                                   kPerInstanceBindingPointBase + i);
  }
  if (push_constant_info_.has_value()) {
    for (const auto& info : push_constant_info_.value().infos) {
      info.push_constant->Flush(
          command_buffer, pipeline_->layout(), frame, info.target_offset,
          push_constant_info_.value().shader_stage);
    }
  }
  for (int mesh_index = 0; mesh_index < mesh_textures_.size(); ++mesh_index) {
    descriptors_[frame][mesh_index]->Bind(command_buffer, pipeline_->layout());
    vertex_buffer_->Draw(command_buffer, mesh_index, instance_count);
  }
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
