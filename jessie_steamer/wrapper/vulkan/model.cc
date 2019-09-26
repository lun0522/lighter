//
//  model.cc
//
//  Created by Pujun Lun on 3/22/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/model.h"

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

using common::VertexAttrib3D;
using VertexInfo = PerVertexBuffer::NoShareIndicesDataInfo;

std::unique_ptr<SamplableImage> CreateTexture(
    const SharedBasicContext& context,
    const ModelBuilder::TextureBinding::TextureSource& source) {
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

void CreateTextureInfo(const ModelBuilder::BindingPointMap& binding_map,
                       const model::TexPerMesh& mesh_textures,
                       const model::TexPerMesh& shared_textures,
                       Descriptor::ImageInfoMap* image_info_map,
                       Descriptor::Info* descriptor_info) {
  vector<Descriptor::Info::Binding> texture_bindings;

  for (int type_index = 0;
       type_index < static_cast<int>(model::TextureType::kNumType);
       ++type_index) {
    const auto texture_type = static_cast<model::TextureType>(type_index);
    const int num_texture = mesh_textures[type_index].size() +
                            shared_textures[type_index].size();

    if (num_texture != 0) {
      const auto binding_point = binding_map.find(texture_type)->second;

      vector<VkDescriptorImageInfo> descriptor_infos{};
      descriptor_infos.reserve(num_texture);
      for (const auto& texture : mesh_textures[type_index]) {
        descriptor_infos.emplace_back(texture->GetDescriptorInfo());
      }
      for (const auto& texture : shared_textures[type_index]) {
        descriptor_infos.emplace_back(texture->GetDescriptorInfo());
      }

      texture_bindings.emplace_back(Descriptor::Info::Binding{
          binding_point,
          CONTAINER_SIZE(descriptor_infos),
      });
      (*image_info_map)[binding_point] = std::move(descriptor_infos);
    }
  }

  *descriptor_info = Descriptor::Info{
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      VK_SHADER_STAGE_FRAGMENT_BIT,
      std::move(texture_bindings),
  };
}

vector<VkPushConstantRange> CreatePushConstantRanges(
    const model::PushConstantInfo& push_constant_info) {
  vector<VkPushConstantRange> ranges;
  ranges.reserve(push_constant_info.infos.size());
  for (const auto& info : push_constant_info.infos) {
    ranges.emplace_back(VkPushConstantRange{
        push_constant_info.shader_stage,
        info.offset,
        info.push_constant->size_per_frame(),
    });
  }
  return ranges;
}

} /* namespace */

ModelBuilder::ModelBuilder(SharedBasicContext context,
                           int num_frame, bool is_opaque,
                           const ModelResource& resource)
    : context_{std::move(context)},
      num_frame_{num_frame},
      pipeline_builder_{absl::make_unique<PipelineBuilder>(context_)} {
  if (absl::holds_alternative<SingleMeshResource>(resource)) {
    LoadSingleMesh(absl::get<SingleMeshResource>(resource));
  } else if (absl::holds_alternative<MultiMeshResource>(resource)) {
    LoadMultiMesh(absl::get<MultiMeshResource>(resource));
  } else {
    FATAL("Unrecognized variant type");
  }

  uniform_resource_maps_.resize(num_frame_);
  if (is_opaque) {
    pipeline_builder_->enable_depth_test();
  } else {
    pipeline_builder_->enable_alpha_blend();
  }
}

void ModelBuilder::LoadSingleMesh(const SingleMeshResource& resource) {
  // load vertices and indices
  const common::ObjFile file{resource.obj_path, resource.obj_index_base};
  const VertexInfo::PerMeshInfo mesh_info{
      PerVertexBuffer::DataInfo{file.indices},
      PerVertexBuffer::DataInfo{file.vertices},
  };
  vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
      context_, VertexInfo{/*per_mesh_infos=*/{mesh_info}});

  // load textures
  mesh_textures_.emplace_back();
  for (const auto& binding : resource.binding_map) {
    const auto texture_type = binding.first;
    binding_map_[texture_type] = binding.second.binding_point;
    for (const auto& source : binding.second.texture_sources) {
      mesh_textures_.back()[static_cast<int>(texture_type)].emplace_back(
          CreateTexture(context_, source));
    }
  }
}

void ModelBuilder::LoadMultiMesh(const MultiMeshResource& resource) {
  // load vertices and indices
  common::ModelLoader loader{resource.obj_path, resource.tex_path};
  VertexInfo vertex_info;
  vertex_info.per_mesh_infos.reserve(loader.mesh_datas().size());
  for (const auto &mesh_data : loader.mesh_datas()) {
    vertex_info.per_mesh_infos.emplace_back(
        VertexInfo::PerMeshInfo{
            PerVertexBuffer::DataInfo{mesh_data.indices},
            PerVertexBuffer::DataInfo{mesh_data.vertices},
        }
    );
  }
  vertex_buffer_ =
      absl::make_unique<StaticPerVertexBuffer>(context_, vertex_info);

  // load textures
  binding_map_ = resource.binding_map;
  mesh_textures_.reserve(loader.mesh_datas().size());
  for (auto& mesh_data : loader.mesh_datas()) {
    mesh_textures_.emplace_back();
    for (auto& texture : mesh_data.textures) {
      const auto type_index = static_cast<int>(texture.texture_type);
      mesh_textures_.back()[type_index].emplace_back(
          absl::make_unique<SharedTexture>(context_, texture.path));
    }
  }
}

ModelBuilder& ModelBuilder::add_shader(PipelineBuilder::ShaderInfo&& info) {
  shader_infos_.emplace_back(std::move(info));
  return *this;
}

ModelBuilder& ModelBuilder::add_instancing(InstancingInfo&& info) {
  instancing_infos_.emplace_back(std::move(info));
  return *this;
}

ModelBuilder& ModelBuilder::add_uniform_usage(Descriptor::Info&& info) {
  uniform_usages_.emplace_back(std::move(info));
  return *this;
}

ModelBuilder& ModelBuilder::add_uniform_resource(
    uint32_t binding_point, const BufferInfoGenerator& info_gen) {
  for (int frame = 0; frame < num_frame_; ++frame) {
    uniform_resource_maps_[frame][binding_point].emplace_back(info_gen(frame));
  }
  return *this;
}

ModelBuilder& ModelBuilder::set_push_constant(model::PushConstantInfo&& info) {
  push_constant_info_ = std::move(info);
  return *this;
}

ModelBuilder& ModelBuilder::add_shared_texture(model::TextureType type,
                                               const TextureBinding& binding) {
  const auto binding_point = binding.binding_point;
  const auto found = binding_map_.find(type);
  if (found == binding_map_.end()) {
    binding_map_[type] = binding_point;
  } else {
    ASSERT_TRUE(found->second == binding_point,
                absl::StrFormat("Extra textures of type %d is bound to point "
                                "%d, but mesh textures of same type are bound "
                                "to point %d",
                                type, binding_point, found->second));
  }
  for (const auto& source : binding.texture_sources) {
    shared_textures_[static_cast<int>(type)].emplace_back(
        CreateTexture(context_, source));
  }
  return *this;
}

std::vector<std::vector<std::unique_ptr<StaticDescriptor>>>
ModelBuilder::CreateDescriptors() {
  std::vector<std::vector<std::unique_ptr<StaticDescriptor>>> descriptors;
  descriptors.resize(num_frame_);
  auto descriptor_infos = uniform_usages_;
  for (int frame = 0; frame < num_frame_; ++frame) {
    descriptors[frame].reserve(mesh_textures_.size());

    // for different frames, we get data from different parts of uniform buffer.
    // for different meshes, we bind different textures. so we need a 2D array
    // of descriptors.
    for (const auto& mesh_texture : mesh_textures_) {
      Descriptor::ImageInfoMap image_info_map;
      Descriptor::Info texture_info;
      CreateTextureInfo(binding_map_, mesh_texture, shared_textures_,
                        &image_info_map, &texture_info);

      descriptor_infos.emplace_back(std::move(texture_info));
      descriptors[frame].emplace_back(
          absl::make_unique<StaticDescriptor>(context_, descriptor_infos));
      descriptor_infos.resize(descriptor_infos.size() - 1);

      // TODO: Derive descriptor type.
      descriptors[frame].back()->UpdateBufferInfos(
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniform_resource_maps_[frame]);
      descriptors[frame].back()->UpdateImageInfos(
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, image_info_map);
    }
  }
  return descriptors;
}

std::unique_ptr<Model> ModelBuilder::Build() {
  auto descriptors = CreateDescriptors();

  vector<const PerInstanceBuffer*> per_instance_buffers;
  vector<VertexInputBinding> bindings{GetPerVertexBindings<VertexAttrib3D>()};
  vector<VertexInputAttribute> attributes{
    GetVertexAttributes<VertexAttrib3D>()};

  for (int i = 0; i < instancing_infos_.size(); ++i) {
    auto& info = instancing_infos_[i];
    ASSERT_NON_NULL(info.per_instance_buffer,
                    "Per instance buffer not provided");
    per_instance_buffers.emplace_back(info.per_instance_buffer);
    bindings.emplace_back(VertexInputBinding{
        kPerInstanceBindingPointBase + i,
        info.data_size,
        /*instancing=*/true,
    });
    attributes.emplace_back(VertexInputAttribute{
        kPerInstanceBindingPointBase + i,
        info.per_instance_attribs,
    });
  }

  (*pipeline_builder_)
      .set_vertex_input(GetBindingDescriptions(bindings),
                        GetAttributeDescriptions(attributes))
      .set_layout({descriptors[0][0]->layout()},
                  push_constant_info_.has_value()
                      ? CreatePushConstantRanges(push_constant_info_.value())
                      : vector<VkPushConstantRange>{});

  instancing_infos_.clear();
  uniform_resource_maps_.clear();
  binding_map_.clear();

  return std::unique_ptr<Model>{new Model{
    context_, std::move(shader_infos_), std::move(vertex_buffer_),
    std::move(per_instance_buffers), std::move(push_constant_info_),
    std::move(shared_textures_), std::move(mesh_textures_),
    std::move(descriptors), std::move(pipeline_builder_)}};
}

void Model::Update(VkExtent2D frame_size, VkSampleCountFlagBits sample_count,
                   const RenderPass& render_pass, uint32_t subpass_index) {
  (*pipeline_builder_)
      .set_viewport({
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
          },
      })
      .set_render_pass(*render_pass, subpass_index)
      .set_sample_count(sample_count);
  for (const auto& info : shader_infos_) {
    pipeline_builder_->add_shader(info);
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
          command_buffer, pipeline_->layout(), frame, info.offset,
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
