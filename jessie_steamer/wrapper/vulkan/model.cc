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
using std::vector;

} /* namespace */

void Model::Init(SharedContext context,
                 unsigned int obj_index_base,
                 const string& obj_path,
                 const std::unordered_map<TextureType, Binding>& bindings,
                 const UniformBuffer& uniform_buffer,
                 const Descriptor::Info& uniform_info,
                 size_t num_frame) {
  // load vertices and indices
  vector<VertexAttrib3D> vertices;
  vector<uint32_t> indices;
  common::util::LoadObjFromFile(obj_path, obj_index_base, &vertices, &indices);

  VertexBuffer::Info vertex_info{
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
  vertex_buffer_.Init(context, {vertex_info});

  // load textures
  Mesh mesh;
  vector<Descriptor::Info::Binding> texture_bindings;
  vector<vector<VkDescriptorImageInfo>> image_infos;
  for (const auto& binding : bindings) {
    TextureType type = binding.first;
    uint32_t binding_point = binding.second.binding_point;
    const vector<string>& texture_paths = binding.second.texture_paths;

    mesh.textures[type].resize(texture_paths.size());
    vector<VkDescriptorImageInfo> temp_infos(texture_paths.size());
    for (size_t i = 0; i < texture_paths.size(); ++i) {
      mesh.textures[type][i].Init(context, {texture_paths[i]});
      temp_infos[i] = mesh.textures[type][i].descriptor_info();
    }
    texture_bindings.emplace_back(binding_point, CONTAINER_SIZE(texture_paths));
    image_infos.emplace_back(move(temp_infos));
  }
  meshes_.emplace_back(move(mesh));

  // create descriptors
  Descriptor::Info texture_info{
      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
      move(texture_bindings),
  };
  vector<Descriptor::Info> infos{
      uniform_info,
      texture_info,
  };

  descriptors_.resize(num_frame);
  for (size_t frame = 0; frame < num_frame; ++frame) {
    descriptors_[frame].Init(context, infos);
    descriptors_[frame].UpdateBufferInfos(
        uniform_info, {uniform_buffer.descriptor_info(frame)});
    descriptors_[frame].UpdateImageInfos(texture_info, image_infos);
  }
}

void Model::Init(SharedContext context,
                 const string& obj_path,
                 const string& tex_path,
                 const std::unordered_map<TextureType, Binding>& bindings,
                 const UniformBuffer& uniform_buffer,
                 const Descriptor::Info& uniform_info,
                 size_t num_frame) {
  // load vertices and indices
  common::ModelLoader loader{obj_path, tex_path, /*flip_uvs=*/false};

  vector<VertexBuffer::Info> vertex_infos;
  vertex_infos.reserve(loader.meshes().size());
  for (const auto& mesh : loader.meshes()) {
    vertex_infos.emplace_back(VertexBuffer::Info{
        /*vertices=*/{
            mesh.vertices.data(),
            sizeof(mesh.vertices[0]) * mesh.vertices.size(),
            CONTAINER_SIZE(mesh.vertices),
        },
        /*indices=*/{
            mesh.indices.data(),
            sizeof(mesh.indices[0]) * mesh.indices.size(),
            CONTAINER_SIZE(mesh.indices),
        },
    });
  }
  vertex_buffer_.Init(context, vertex_infos);

  // load textures
  meshes_.reserve(loader.meshes().size());
  vector<Descriptor::Info::Binding> texture_bindings;
  vector<vector<VkDescriptorImageInfo>> image_infos;
  for (auto& loaded_mesh : loader.meshes()) {
    Mesh mesh;
    for (auto& loaded_tex : loaded_mesh.textures) {
      vector<common::util::Image> images;
      images.emplace_back(move(loaded_tex.image));
      TextureImage texture;
      texture.Init(context, images);
      mesh.textures[loaded_tex.type].emplace_back(move(texture));
    }

    for (int type = 0; type < TextureType::kTypeMaxEnum; ++type) {
      if (mesh.textures[type].empty()) {
        continue;
      }

      const auto binding = bindings.find(static_cast<TextureType>(type));
      if (binding == bindings.end()) {
        throw std::runtime_error{"Texture type not handled: " +
                                 std::to_string(type)};
      }

      uint32_t binding_point = binding->second.binding_point;
      texture_bindings.emplace_back(
          binding_point, CONTAINER_SIZE(mesh.textures[type]));

      vector<VkDescriptorImageInfo> temp_infos;
      for (const auto& texture : mesh.textures[type]) {
        temp_infos.emplace_back(texture.descriptor_info());
      }
      image_infos.emplace_back(move(temp_infos));
    }

    meshes_.emplace_back(move(mesh));
  }

  // create descriptors
  Descriptor::Info texture_info{
      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
      move(texture_bindings),
  };
  vector<Descriptor::Info> infos{
      uniform_info,
      texture_info,
  };

  descriptors_.resize(num_frame);
  for (size_t frame = 0; frame < num_frame; ++frame) {
    descriptors_[frame].Init(context, infos);
    descriptors_[frame].UpdateBufferInfos(
     uniform_info, {uniform_buffer.descriptor_info(frame)});
    descriptors_[frame].UpdateImageInfos(texture_info, image_infos);
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
