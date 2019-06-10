//
//  model.cc
//
//  Created by Pujun Lun on 3/22/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/model.h"

#include <algorithm>
#include <stdexcept>

#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/macro.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using absl::optional;
using common::VertexAttrib3D;
using std::move;
using std::runtime_error;
using std::vector;

struct VertexInputBinding {
  uint32_t binding_point;
  uint32_t data_size;
  bool instancing;
};

struct VertexInputAttribute {
  uint32_t binding_point;
  vector<Model::VertexAttribute> attributes;
};

template <typename VertexType>
VertexInputBinding GetPerVertexBindings() {
  return VertexInputBinding{
      buffer::kPerVertexBindingPoint,
      /*data_size=*/static_cast<uint32_t>(sizeof(VertexType)),
      /*instancing=*/false,
  };
}

template <typename VertexType>
VertexInputAttribute GetVertexAttributes() {
  throw runtime_error{"Vertex type not recognized"};
}

template <>
VertexInputAttribute GetVertexAttributes<VertexAttrib3D>() {
  return VertexInputAttribute{
      buffer::kPerVertexBindingPoint,
      /*attributes=*/{
          Model::VertexAttribute{
              /*location=*/0,
              /*offset=*/static_cast<uint32_t>(
                  offsetof(VertexAttrib3D, pos)),
              /*format=*/VK_FORMAT_R32G32B32_SFLOAT,
          },
          Model::VertexAttribute{
              /*location=*/1,
              /*offset=*/static_cast<uint32_t>(
                  offsetof(VertexAttrib3D, norm)),
              /*format=*/VK_FORMAT_R32G32B32_SFLOAT,
          },
          Model::VertexAttribute{
              /*location=*/2,
              /*offset=*/static_cast<uint32_t>(
                  offsetof(VertexAttrib3D, tex_coord)),
              /*format=*/VK_FORMAT_R32G32_SFLOAT,
          },
      },
  };
}

vector<VkVertexInputBindingDescription> GetBindingDescriptions(
    const vector<VertexInputBinding>& bindings) {
  vector<VkVertexInputBindingDescription> descriptions;
  descriptions.reserve(bindings.size());
  for (const auto& binding : bindings) {
    descriptions.emplace_back(VkVertexInputBindingDescription{
        binding.binding_point,
        binding.data_size,
        binding.instancing ? VK_VERTEX_INPUT_RATE_INSTANCE :
                             VK_VERTEX_INPUT_RATE_VERTEX,
    });
  }
  return descriptions;
}

vector<VkVertexInputAttributeDescription> GetAttributeDescriptions(
    const vector<VertexInputAttribute>& attributes) {
  int num_attribute = 0;
  for (const auto& attribs : attributes) {
    num_attribute += attribs.attributes.size();
  }

  vector<VkVertexInputAttributeDescription> descriptions;
  descriptions.reserve(num_attribute);
  for (const auto& attribs : attributes) {
    for (const auto& attrib : attribs.attributes) {
      descriptions.emplace_back(VkVertexInputAttributeDescription{
          attrib.location,
          attribs.binding_point,
          attrib.format,
          attrib.offset,
      });
    }
  }
  return descriptions;
}

PerVertexBuffer::Info CreateVertexInfo(const vector<VertexAttrib3D>& vertices,
                                       const vector<uint32_t>& indices) {
  return PerVertexBuffer::Info{
      /*vertices=*/{
          vertices.data(),
          sizeof(vertices[0]) * vertices.size(),
          CONTAINER_SIZE(vertices),
      },
      /*indices=*/{
          indices.data(),
          sizeof(indices[0]) * indices.size(),
          CONTAINER_SIZE(indices),
      },
  };
}

void CreateTextureInfo(const Model::Mesh& mesh,
                       const Model::FindBindingPoint& find_binding_point,
                       Descriptor::ImageInfos* image_infos,
                       Descriptor::Info* texture_info) {
  vector<Descriptor::Info::Binding> texture_bindings;

  for (int type = 0; type < Model::ResourceType::kNumTextureType; ++type) {
    const auto resource_type = static_cast<Model::ResourceType>(type);
    if (!mesh[type].empty()) {
      const uint32_t binding_point = find_binding_point(resource_type);

      vector<VkDescriptorImageInfo> descriptor_infos{};
      descriptor_infos.reserve(mesh[type].size());
      for (const auto& image : mesh[type]) {
        descriptor_infos.emplace_back(image->descriptor_info());
      }
      (*image_infos)[binding_point] = move(descriptor_infos);

      texture_bindings.emplace_back(Descriptor::Info::Binding{
          resource_type,
          binding_point,
          CONTAINER_SIZE(mesh[type]),
      });
    }
  }

  *texture_info = {
      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
      move(texture_bindings),
  };
}

vector<VkPushConstantRange> CreatePushConstantRanges(
    const optional<Model::PushConstantInfos>& push_constant_infos) {
  vector<VkPushConstantRange> ranges;
  if (push_constant_infos.has_value()) {
    for (const auto& push_constants : push_constant_infos.value()) {
      for (const auto& info : push_constants.infos) {
        ranges.emplace_back(VkPushConstantRange{
            push_constants.shader_stage,
            info.offset,
            info.size(),
        });
      }
    }
  }
  return ranges;
}

} /* namespace */

void Model::Init(const vector<PipelineBuilder::ShaderInfo>& shader_infos,
                 const ModelResource& resource,
                 const optional<UniformInfos>& uniform_infos,
                 const optional<InstancingInfo>& instancing_info,
                 const optional<PushConstantInfos>& push_constant_infos,
                 const PipelineBuilder::RenderPassInfo& render_pass_info,
                 VkExtent2D frame_size, int num_frame, bool is_opaque) {
  if (is_first_time_) {
    is_first_time_ = false;

    push_constant_infos_ = push_constant_infos;

    if (instancing_info.has_value()) {
      if (instancing_info.value().per_instance_buffer == nullptr) {
        throw runtime_error{"Per instance buffer not provided"};
      }
      per_instance_buffer_ = instancing_info.value().per_instance_buffer;
    }

    FindBindingPoint find_binding_point;
    if (absl::holds_alternative<SingleMeshResource>(resource)) {
      find_binding_point =
          LoadSingleMesh(absl::get<SingleMeshResource>(resource));
    } else if (absl::holds_alternative<MultiMeshResource>(resource)) {
      find_binding_point =
          LoadMultiMesh(absl::get<MultiMeshResource>(resource));
    } else {
      throw runtime_error{"Unrecognized variant type"};
    }
    CreateDescriptors(find_binding_point, uniform_infos, num_frame);

    vector<VertexInputBinding> bindings{GetPerVertexBindings<VertexAttrib3D>()};
    vector<VertexInputAttribute> attributes{
        GetVertexAttributes<VertexAttrib3D>()};

    if (instancing_info.has_value()) {
      bindings.emplace_back(VertexInputBinding{
          buffer::kPerInstanceBindingPoint,
          instancing_info.value().data_size,
          /*instancing=*/true,
      });

      attributes.emplace_back(VertexInputAttribute{
          buffer::kPerInstanceBindingPoint,
          instancing_info.value().per_instance_attribs,
      });
    }

    pipeline_builder_
        .set_vertex_input(GetBindingDescriptions(bindings),
                          GetAttributeDescriptions(attributes))
        .set_layout({descriptors_[0][0]->layout()},
                    CreatePushConstantRanges(push_constant_infos_));
    if (!is_opaque) {
      pipeline_builder_.enable_alpha_blend().disable_depth_test();
    }
  }

  // create pipeline
  pipeline_builder_
      .set_viewport({
          /*x=*/0.0f,
          /*y=*/0.0f,
          static_cast<float>(frame_size.width),
          static_cast<float>(frame_size.height),
          /*minDepth=*/0.0f,
          /*maxDepth=*/1.0f})
      .set_scissor({
          /*offset=*/{0, 0},
          frame_size})
      .set_render_pass(render_pass_info);
  for (const auto& info : shader_infos) {
    pipeline_builder_.add_shader(info);
  }
  pipeline_ = pipeline_builder_.Build();
}

Model::FindBindingPoint Model::LoadSingleMesh(
    const SingleMeshResource& resource) {
  // load vertices and indices
  common::ObjFile file{resource.obj_path, resource.obj_index_base};
  vertex_buffer_.Init({CreateVertexInfo(file.vertices, file.indices)});

  // load textures
  meshes_.emplace_back();
  for (const auto &binding : resource.binding_map) {
    const ResourceType type = binding.first;
    const vector<TextureImage::SourcePath>& texture_paths =
        binding.second.texture_paths;

    for (const auto& path : texture_paths) {
      meshes_.back()[type].emplace_back(
          TextureImage::GetTexture(context_, path));
    }
  }

  return [&resource](ResourceType type) {
    return resource.binding_map.find(type)->second.binding_point;
  };
}

Model::FindBindingPoint Model::LoadMultiMesh(
    const MultiMeshResource& resource) {
  // load vertices and indices
  common::ModelLoader loader{resource.obj_path, resource.tex_path};
  vector<PerVertexBuffer::Info> vertex_infos;
  vertex_infos.reserve(loader.meshes().size());
  for (const auto &mesh : loader.meshes()) {
    vertex_infos.emplace_back(CreateVertexInfo(mesh.vertices, mesh.indices));
  }
  vertex_buffer_.Init(vertex_infos);

  // load textures
  BindingPointMap binding_map = resource.binding_map;
  if (resource.extra_texture_map.has_value()) {
    for (const auto &binding : resource.extra_texture_map.value()) {
      const ResourceType type = binding.first;
      const uint32_t binding_point = binding.second.binding_point;

      const auto found = resource.binding_map.find(type);
      if (found == resource.binding_map.end()) {
        binding_map[type] = binding_point;
      } else if (found->second != binding_point) {
        throw runtime_error{absl::StrFormat(
            "Extra textures of type %d is bound to point %d, but mesh textures "
            "of same type are bound to point %d",
            type, binding_point, found->second)};
      }
    }
  }

  meshes_.reserve(loader.meshes().size());
  for (auto& mesh : loader.meshes()) {
    meshes_.emplace_back();
    for (auto& texture : mesh.textures) {
      meshes_.back()[texture.resource_type].emplace_back(
          TextureImage::GetTexture(context_, texture.path));
    }

    if (resource.extra_texture_map.has_value()) {
      for (const auto& binding : resource.extra_texture_map.value()) {
        const ResourceType type = binding.first;
        const vector<TextureImage::SourcePath>& texture_paths =
            binding.second.texture_paths;

        for (const auto& path : texture_paths) {
          meshes_.back()[type].emplace_back(
              TextureImage::GetTexture(context_, path));
        }
      }
    }
  }

  return [binding_map](ResourceType type) {
    return binding_map.find(type)->second;
  };
}

void Model::CreateDescriptors(const FindBindingPoint& find_binding_point,
                              const optional<UniformInfos>& uniform_infos,
                              int num_frame) {
  descriptors_.resize(num_frame);
  for (int frame = 0; frame < num_frame; ++frame) {
    descriptors_[frame].reserve(meshes_.size());

    // for different frames, we get data from different parts of uniform buffer.
    // for different meshes, we bind different textures. so we need a 2D array
    // of descriptors.
    for (const auto& mesh : meshes_) {
      Descriptor::ImageInfos image_infos;
      Descriptor::Info texture_info;
      CreateTextureInfo(mesh, find_binding_point, &image_infos, &texture_info);
      const VkDescriptorType tex_desc_type = texture_info.descriptor_type;

      vector<Descriptor::Info> descriptor_infos{move(texture_info)};
      if (uniform_infos.has_value()) {
        for (const auto& uniform_info : uniform_infos.value()) {
          descriptor_infos.emplace_back(uniform_info.second);
        }
      }

      descriptors_[frame].emplace_back(
          absl::make_unique<Descriptor>(context_, descriptor_infos));
      if (uniform_infos.has_value()) {
        for (const auto& uniform_info : uniform_infos.value()) {
          descriptors_[frame].back()->UpdateBufferInfos(
            uniform_info.second, {uniform_info.first.descriptor_info(frame)});
        }
      }
      descriptors_[frame].back()->UpdateImageInfos(tex_desc_type, image_infos);
    }
  }
}

void Model::Draw(const VkCommandBuffer& command_buffer,
                 int frame, uint32_t instance_count) const {
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    **pipeline_);
  if (per_instance_buffer_) {
    per_instance_buffer_->Bind(command_buffer);
  }
  if (push_constant_infos_.has_value()) {
    for (const auto& push_constants : push_constant_infos_.value()) {
      for (const auto& info : push_constants.infos) {
        vkCmdPushConstants(command_buffer, pipeline_->layout(),
                           push_constants.shader_stage,
                           info.offset, info.size(), info.data(frame));
      }
    }
  }
  for (int mesh_index = 0; mesh_index < meshes_.size(); ++mesh_index) {
    vkCmdBindDescriptorSets(
        command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_->layout(), /*firstSet=*/0, /*descriptorSetCount=*/1,
        &descriptors_[frame][mesh_index]->set(), /*dynamicOffsetCount=*/0,
        /*pDynamicOffsets=*/nullptr);
    vertex_buffer_.Draw(command_buffer, mesh_index, instance_count);
  }
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
