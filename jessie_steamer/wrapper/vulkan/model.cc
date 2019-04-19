//
//  model.cc
//
//  Created by Pujun Lun on 3/22/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/model.h"

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
using std::unordered_map;
using std::vector;

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

vector<vector<VkDescriptorImageInfo>> CreateImageInfos(
    const vector<Model::Mesh>& meshes,
    const Model::BindingMap& bindings) {
  std::array<vector<VkDescriptorImageInfo>,
             Model::TextureType::kTypeMaxEnum> temp_infos;
  for (int type = 0; type < Model::TextureType::kTypeMaxEnum; ++type) {
    temp_infos[type].reserve(meshes[0].textures[type].size() * meshes.size());
    for (const auto& mesh : meshes) {
      const vector<TextureImage>& images = mesh.textures[type];
      for (const auto& image : images) {
        temp_infos[type].emplace_back(image.descriptor_info());
      }
    }
  }

  vector<vector<VkDescriptorImageInfo>> image_infos;
  image_infos.reserve(bindings.size());
  for (const auto& binding : bindings) {
    const Model::TextureType type = binding.first;
    if (!temp_infos[type].empty()) {
      image_infos.emplace_back(move(temp_infos[type]));
    }
  }
  return image_infos;
}

} /* namespace */

void Model::Init(SharedContext context,
                 unsigned int obj_index_base,
                 const string& obj_path,
                 const BindingMap& bindings,
                 const vector<UniformInfo>& uniform_infos,
                 size_t num_frame) {
  // load vertices and indices
  vector<VertexAttrib3D> vertices;
  vector<uint32_t> indices;
  common::util::LoadObjFromFile(obj_path, obj_index_base, &vertices, &indices);
  vertex_buffer_.Init(context, {CreateVertexInfo(vertices, indices)});

  // load textures
  vector<Descriptor::Info::Binding> texture_bindings;
  meshes_.resize(1);
  for (const auto& binding : bindings) {
    const TextureType type = binding.first;
    const vector<vector<string>>& texture_paths = binding.second.texture_paths;

    meshes_[0].textures[type].resize(texture_paths.size());
    for (size_t i = 0; i < texture_paths.size(); ++i) {
      meshes_[0].textures[type][i].Init(context, texture_paths[i]);
    }
    texture_bindings.emplace_back(type, CONTAINER_SIZE(texture_paths));
  }

  // create descriptors
  Descriptor::Info texture_info{
      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
      move(texture_bindings),
  };
  vector<vector<VkDescriptorImageInfo>> image_infos =
      CreateImageInfos(meshes_, bindings);
  vector<Descriptor::Info> descriptor_infos{texture_info};
  for (const auto& uniform_info : uniform_infos) {
    descriptor_infos.emplace_back(*uniform_info.second);
  }

  descriptors_.resize(num_frame);
  for (size_t frame = 0; frame < num_frame; ++frame) {
    descriptors_[frame].Init(context, descriptor_infos);
    for (const auto& uniform_info : uniform_infos) {
      descriptors_[frame].UpdateBufferInfos(
          *uniform_info.second, {uniform_info.first->descriptor_info(frame)});
    }
    descriptors_[frame].UpdateImageInfos(texture_info, image_infos);
  }
}

void Model::Init(SharedContext context,
                 const string& obj_path,
                 const string& tex_path,
                 const BindingMap& bindings,
                 const vector<UniformInfo>& uniform_infos,
                 size_t num_frame) {
  // load vertices and indices
  common::ModelLoader loader{obj_path, tex_path, /*flip_uvs=*/false};
  vector<VertexBuffer::Info> vertex_infos;
  vertex_infos.reserve(loader.meshes().size());
  for (const auto& mesh : loader.meshes()) {
    vertex_infos.emplace_back(CreateVertexInfo(mesh.vertices, mesh.indices));
  }
  vertex_buffer_.Init(context, vertex_infos);

//  // load textures
//  vector<Descriptor::Info::Binding> texture_bindings;
//  meshes_.reserve(loader.meshes().size());
//  for (auto& loaded_mesh : loader.meshes()) {
//    Mesh mesh;
//    for (auto& loaded_tex : loaded_mesh.textures) {
//      vector<common::util::Image> images;
//      images.emplace_back(move(loaded_tex.image));
//      TextureImage texture;
//      texture.Init(context, images);
//      mesh.textures[loaded_tex.type].emplace_back(move(texture));
//    }
//    meshes_.emplace_back(move(mesh));
//  }
//  for (int type = 0; type < TextureType::kTypeMaxEnum; ++type) {
//    if (!meshes_[0].textures[type].empty()) {
//      texture_bindings.emplace_back(
//          type, CONTAINER_SIZE(meshes_[0].textures[type]));
//    }
//  }

  // create descriptors
//  Descriptor::Info texture_info{
//      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//      /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
//      move(texture_bindings),
//  };
//  vector<vector<VkDescriptorImageInfo>> image_infos =
//      CreateImageInfos(meshes_, bindings);
//  vector<Descriptor::Info> descriptor_infos{texture_info};
  vector<Descriptor::Info> descriptor_infos;
  for (const auto& uniform_info : uniform_infos) {
    descriptor_infos.emplace_back(*uniform_info.second);
  }

  descriptors_.resize(num_frame);
  for (size_t frame = 0; frame < num_frame; ++frame) {
    descriptors_[frame].Init(context, descriptor_infos);
    for (const auto& uniform_info : uniform_infos) {
      descriptors_[frame].UpdateBufferInfos(
        *uniform_info.second, {uniform_info.first->descriptor_info(frame)});
    }
//    descriptors_[frame].UpdateImageInfos(texture_info, image_infos);
  }
}

const vector<VkVertexInputBindingDescription>& Model::binding_descs() {
  static const vector<VkVertexInputBindingDescription> descriptions{
      VkVertexInputBindingDescription{
          /*binding=*/0,
          /*stride=*/sizeof(VertexAttrib3D),
          // for instancing, use _INSTANCE for .inputRate
          /*inputRate=*/VK_VERTEX_INPUT_RATE_VERTEX,
      },
  };
  return descriptions;
}

const vector<VkVertexInputAttributeDescription>& Model::attrib_descs() {
  static const vector<VkVertexInputAttributeDescription> descriptions{
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
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
