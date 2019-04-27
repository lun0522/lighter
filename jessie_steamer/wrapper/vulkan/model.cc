//
//  model.cc
//
//  Created by Pujun Lun on 3/22/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/model.h"

#include <algorithm>
#include <stdexcept>

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/context.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using common::util::VertexAttrib3D;
using std::move;
using std::string;
using std::unique_ptr;
using std::vector;

const vector<VkVertexInputBindingDescription>& binding_descs() {
  const static vector<VkVertexInputBindingDescription> descriptions{{
      /*binding=*/0,
      /*stride=*/sizeof(VertexAttrib3D),
      // for instancing, use _INSTANCE for .inputRate
      /*inputRate=*/VK_VERTEX_INPUT_RATE_VERTEX,
  }};
  return descriptions;
};

const vector<VkVertexInputAttributeDescription>& attrib_descs() {
  const static vector<VkVertexInputAttributeDescription> descriptions{
      VkVertexInputAttributeDescription{
          /*location=*/0,  // layout (location = 0) in
          /*binding=*/0,  // which binding point does data come from
          /*format=*/VK_FORMAT_R32G32B32_SFLOAT,  // implies total size
          /*offset=*/static_cast<uint32_t>(offsetof(VertexAttrib3D, pos)),
      },
      VkVertexInputAttributeDescription{
          /*location=*/1,
          /*binding=*/0,
          /*format=*/VK_FORMAT_R32G32B32_SFLOAT,
          /*offset=*/static_cast<uint32_t>(offsetof(VertexAttrib3D, norm)),
      },
      VkVertexInputAttributeDescription{
          /*location=*/2,
          /*binding=*/0,
          /*format=*/VK_FORMAT_R32G32_SFLOAT,
          /*offset=*/static_cast<uint32_t>(offsetof(VertexAttrib3D, tex_coord)),
      },
  };
  return descriptions;
};

VertexBuffer::Info CreateVertexInfo(const vector<VertexAttrib3D>& vertices,
                                    const vector<uint32_t>& indices) {
  return VertexBuffer::Info{
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
};

void CreateTextureInfo(const Model::Mesh& mesh,
                       const Model::FindBindingPoint& find_binding_point,
                       Descriptor::ImageInfos* image_infos,
                       Descriptor::Info* texture_info) {
  vector<Descriptor::Info::Binding> texture_bindings;

  for (int type = 0; type < Model::TextureType::kTypeMaxEnum; ++type) {
    const auto texture_type = static_cast<Model::TextureType>(type);
    if (!mesh[type].empty()) {
      const uint32_t binding_point = find_binding_point(texture_type);

      vector<VkDescriptorImageInfo> descriptor_infos{};
      descriptor_infos.reserve(mesh[type].size());
      for (const auto& image : mesh[type]) {
        descriptor_infos.emplace_back(image.descriptor_info());
      }
      image_infos->operator[](binding_point) = move(descriptor_infos);

      texture_bindings.emplace_back(Descriptor::Info::Binding{
          texture_type,
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

} /* namespace */

void Model::Init(SharedContext context,
                 const vector<PipelineBuilder::ShaderInfo>& shader_infos,
                 const vector<UniformInfo>& uniform_infos,
                 const ModelResource& resource,
                 size_t num_frame) {
  if (is_first_time_) {
    context_ = move(context);

    FindBindingPoint find_binding_point;
    if (absl::holds_alternative<SingleMeshResource>(resource)) {
      find_binding_point =
          LoadSingleMesh(absl::get<SingleMeshResource>(resource));
    } else if (absl::holds_alternative<MultiMeshResource>(resource)) {
      find_binding_point =
          LoadMultiMesh(absl::get<MultiMeshResource>(resource));
    } else {
      throw std::runtime_error{"Unrecognized variant type"};
    }
    CreateDescriptors(find_binding_point, uniform_infos, num_frame);

    pipeline_builder_.Init(context_)
        .set_vertex_input(binding_descs(), attrib_descs())
        .set_layout({descriptors_[0][0]->layout()});

    is_first_time_ = false;
  }

  // create pipeline
  VkExtent2D target_extent = context_->swapchain().extent();
  pipeline_builder_.set_viewport({
      /*x=*/0.0f,
      /*y=*/0.0f,
      static_cast<float>(target_extent.width),
      static_cast<float>(target_extent.height),
      /*minDepth=*/0.0f,
      /*maxDepth=*/1.0f,
      }).set_scissor({
      /*offset=*/{0, 0},
      target_extent,
  });
  for (const auto& info : shader_infos) {
    pipeline_builder_.add_shader(info);
  }
  pipeline_ = pipeline_builder_.Build();
}

Model::FindBindingPoint Model::LoadSingleMesh(
    const SingleMeshResource& resource) {
  // load vertices and indices
  vector<VertexAttrib3D> vertices;
  vector<uint32_t> indices;
  common::util::LoadObjFromFile(resource.obj_path, resource.obj_index_base,
                                &vertices, &indices);
  vertex_buffer_.Init(context_, {CreateVertexInfo(vertices, indices)});

  // load textures
  meshes_.emplace_back();
  for (const auto &binding : resource.binding_map) {
    const TextureType type = binding.first;
    const vector<vector<string>> &texture_paths = binding.second.texture_paths;

    meshes_.back()[type].resize(texture_paths.size());
    for (size_t i = 0; i < texture_paths.size(); ++i) {
      meshes_.back()[type][i].Init(context_, texture_paths[i]);
    }
  }

  return [&resource](TextureType type) {
    return resource.binding_map.find(type)
      ->second.binding_point;
  };
}

Model::FindBindingPoint Model::LoadMultiMesh(
    const MultiMeshResource& resource) {
  // load vertices and indices
  common::ModelLoader loader{resource.obj_path, resource.tex_path,
                             /*flip_uvs=*/false};
  vector<VertexBuffer::Info> vertex_infos;
  vertex_infos.reserve(loader.meshes().size());
  for (const auto &mesh : loader.meshes()) {
    vertex_infos.emplace_back(CreateVertexInfo(mesh.vertices, mesh.indices));
  }
  vertex_buffer_.Init(context_, vertex_infos);

  // load textures
  meshes_.reserve(loader.meshes().size());
  for (auto &loaded_mesh : loader.meshes()) {
    meshes_.emplace_back();
    for (auto &loaded_tex : loaded_mesh.textures) {
      vector<common::util::Image> images;
      images.emplace_back(move(loaded_tex.image));
      meshes_.back()[loaded_tex.type].emplace_back();
      meshes_.back()[loaded_tex.type].back().Init(context_, images);
    }
  }

  return [&resource](TextureType type) {
    return resource.binding_map.find(type)->second;
  };
}

void Model::CreateDescriptors(const FindBindingPoint& find_binding_point,
                              const vector<UniformInfo>& uniform_infos,
                              size_t num_frame) {
  descriptors_.resize(num_frame);
  for (size_t frame = 0; frame < num_frame; ++frame) {
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
      for (const auto& uniform_info : uniform_infos) {
        descriptor_infos.emplace_back(*uniform_info.second);
      }

      descriptors_[frame].emplace_back(new Descriptor);
      descriptors_[frame].back()->Init(context_, descriptor_infos);
      for (const auto& uniform_info : uniform_infos) {
        descriptors_[frame].back()->UpdateBufferInfos(
            *uniform_info.second, {uniform_info.first->descriptor_info(frame)});
      }
      descriptors_[frame].back()->UpdateImageInfos(tex_desc_type, image_infos);
    }
  }
}

void Model::Draw(const VkCommandBuffer& command_buffer,
                 size_t frame) const {
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    **pipeline_);
  for (size_t mesh_index = 0; mesh_index < meshes_.size(); ++mesh_index) {
    vkCmdBindDescriptorSets(
        command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_->layout(), /*firstSet=*/0, /*descriptorSetCount=*/1,
        &descriptors_[frame][mesh_index]->set(), /*dynamicOffsetCount=*/0,
        /*pDynamicOffsets=*/nullptr);
    vertex_buffer_.Draw(command_buffer, mesh_index);
  }
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
