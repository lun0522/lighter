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
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

using common::VertexAttrib3D;
using VertexInfo = PerVertexBuffer::InfoNoReuse;

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
                       Descriptor::ImageInfos* image_infos,
                       Descriptor::Info* texture_info) {
  vector<Descriptor::Info::Binding> texture_bindings;

  for (int type = 0; type < model::ResourceType::kNumTextureType; ++type) {
    const auto resource_type = static_cast<model::ResourceType>(type);
    const int num_texture = mesh_textures[type].size() +
                            shared_textures[type].size();

    if (num_texture != 0) {
      const auto binding_point = binding_map.find(resource_type)->second;

      vector<VkDescriptorImageInfo> descriptor_infos{};
      descriptor_infos.reserve(num_texture);
      for (const auto& texture : mesh_textures[type]) {
        descriptor_infos.emplace_back(texture->descriptor_info());
      }
      for (const auto& texture : shared_textures[type]) {
        descriptor_infos.emplace_back(texture->descriptor_info());
      }

      texture_bindings.emplace_back(Descriptor::Info::Binding{
          resource_type,
          binding_point,
          CONTAINER_SIZE(descriptor_infos),
      });
      (*image_infos)[binding_point] = std::move(descriptor_infos);
    }
  }

  *texture_info = Descriptor::Info{
      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
      std::move(texture_bindings),
  };
}

vector<VkPushConstantRange> CreatePushConstantRanges(
    const vector<model::PushConstantInfo>& push_constant_infos) {
  vector<VkPushConstantRange> ranges;
  for (const auto& push_constant : push_constant_infos) {
    for (const auto& info : push_constant.infos) {
      ranges.emplace_back(VkPushConstantRange{
          push_constant.shader_stage,
          info.offset,
          info.size(),
      });
    }
  }
  return ranges;
}

} /* namespace */

ModelBuilder::ModelBuilder(SharedBasicContext context,
                           int num_frame, bool is_opaque,
                           const ModelResource& resource)
    : context_{std::move(context)},
      num_frame_{num_frame},
      vertex_buffer_{absl::make_unique<PerVertexBuffer>(context_)},
      pipeline_builder_{absl::make_unique<PipelineBuilder>(context_)} {
  if (absl::holds_alternative<SingleMeshResource>(resource)) {
    LoadSingleMesh(absl::get<SingleMeshResource>(resource));
  } else if (absl::holds_alternative<MultiMeshResource>(resource)) {
    LoadMultiMesh(absl::get<MultiMeshResource>(resource));
  } else {
    FATAL("Unrecognized variant type");
  }
  if (is_opaque) {
    pipeline_builder_->enable_depth_test();
  } else {
    pipeline_builder_->enable_alpha_blend();
  }
}

void ModelBuilder::LoadSingleMesh(const SingleMeshResource& resource) {
  // load vertices and indices
  common::ObjFile file{resource.obj_path, resource.obj_index_base};
  vertex_buffer_->Init(VertexInfo{/*per_mesh_infos=*/{
          VertexInfo::PerMeshInfo{
              /*vertices=*/{file.vertices},
              /*indices=*/{file.indices},
          },
      },
  });

  // load textures
  mesh_textures_.emplace_back();
  for (const auto &binding : resource.binding_map) {
    const auto type = binding.first;
    binding_map_[type] = binding.second.binding_point;
    for (const auto& source : binding.second.texture_sources) {
      mesh_textures_.back()[type].emplace_back(CreateTexture(context_, source));
    }
  }
}

void ModelBuilder::LoadMultiMesh(const MultiMeshResource& resource) {
  // load vertices and indices
  common::ModelLoader loader{resource.obj_path, resource.tex_path};
  VertexInfo vertex_info;
  vertex_info.per_mesh_infos.reserve(loader.meshes().size());
  for (const auto &mesh : loader.meshes()) {
    vertex_info.per_mesh_infos.emplace_back(
        VertexInfo::PerMeshInfo{
            /*vertices=*/{mesh.vertices},
            /*indices=*/{mesh.indices},
        }
    );
  }
  vertex_buffer_->Init(vertex_info);

  // load textures
  binding_map_ = resource.binding_map;
  mesh_textures_.reserve(loader.meshes().size());
  for (auto& mesh : loader.meshes()) {
    mesh_textures_.emplace_back();
    for (auto& texture : mesh.textures) {
      mesh_textures_.back()[texture.resource_type].emplace_back(
          absl::make_unique<SharedTexture>(context_, texture.path));
    }
  }
}

ModelBuilder& ModelBuilder::add_shader(PipelineBuilder::ShaderInfo&& info) {
  shader_infos_.emplace_back(info);
  return *this;
}

ModelBuilder& ModelBuilder::add_instancing(InstancingInfo&& info) {
  instancing_infos_.emplace_back(info);
  return *this;
}

ModelBuilder& ModelBuilder::add_uniform_buffer(UniformInfo&& info) {
  uniform_infos_.emplace_back(info);
  return *this;
}

ModelBuilder& ModelBuilder::add_shared_texture(model::ResourceType type,
                                               const TextureBinding& binding) {
  const auto binding_point = binding.binding_point;
  const auto found = binding_map_.find(type);
  if (found == binding_map_.end()) {
    binding_map_[type] = binding_point;
  } else if (found->second != binding_point) {
    FATAL(absl::StrFormat(
        "Extra textures of type %d is bound to point %d, but mesh textures "
        "of same type are bound to point %d",
        type, binding_point, found->second));
  }
  for (const auto& source : binding.texture_sources) {
    shared_textures_[type].emplace_back(CreateTexture(context_, source));
  }
  return *this;
}

ModelBuilder& ModelBuilder::add_push_constant(model::PushConstantInfo&& info) {
  push_constant_infos_.emplace_back(info);
  return *this;
}

std::vector<std::vector<std::unique_ptr<StaticDescriptor>>>
ModelBuilder::CreateDescriptors() {
  std::vector<std::vector<std::unique_ptr<StaticDescriptor>>> descriptors;
  descriptors.resize(num_frame_);
  for (int frame = 0; frame < num_frame_; ++frame) {
    descriptors[frame].reserve(mesh_textures_.size());

    // for different frames, we get data from different parts of uniform buffer.
    // for different meshes, we bind different textures. so we need a 2D array
    // of descriptors.
    for (const auto& mesh_texture : mesh_textures_) {
      Descriptor::ImageInfos image_infos;
      Descriptor::Info texture_info;
      CreateTextureInfo(binding_map_, mesh_texture, shared_textures_,
                        &image_infos, &texture_info);
      const VkDescriptorType tex_desc_type = texture_info.descriptor_type;

      vector<Descriptor::Info> descriptor_infos{std::move(texture_info)};
      for (const auto& info : uniform_infos_) {
        descriptor_infos.emplace_back(info.second);
      }

      descriptors[frame].emplace_back(
          absl::make_unique<StaticDescriptor>(context_, descriptor_infos));
      for (const auto& info : uniform_infos_) {
        descriptors[frame].back()->UpdateBufferInfos(
            info.second, {info.first->descriptor_info(frame)});
      }
      descriptors[frame].back()->UpdateImageInfos(tex_desc_type, image_infos);
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
    if (info.per_instance_buffer == nullptr) {
      FATAL("Per instance buffer not provided");
    }
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
                  CreatePushConstantRanges(push_constant_infos_));

  instancing_infos_.clear();
  uniform_infos_.clear();
  binding_map_.clear();

  std::unique_ptr<Model> model{new Model{}};
  model->context_ = std::move(context_);
  model->shader_infos_ = std::move(shader_infos_);
  model->vertex_buffer_ = std::move(vertex_buffer_);
  model->per_instance_buffers_ = std::move(per_instance_buffers);
  model->push_constant_infos_ = std::move(push_constant_infos_);
  model->shared_textures_ = std::move(shared_textures_);
  model->mesh_textures_ = std::move(mesh_textures_);
  model->descriptors_ = std::move(descriptors);
  model->pipeline_builder_ = std::move(pipeline_builder_);
  return model;
}

void Model::Update(VkExtent2D frame_size,
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
      .set_render_pass(*render_pass, subpass_index);
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
  for (const auto& push_constant : push_constant_infos_) {
    for (const auto& info : push_constant.infos) {
      vkCmdPushConstants(command_buffer, pipeline_->layout(),
                         push_constant.shader_stage,
                         info.offset, info.size(), info.data(frame));
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
