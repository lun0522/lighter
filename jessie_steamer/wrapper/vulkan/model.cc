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

const vector<VkVertexInputBindingDescription> binding_descs{
    VkVertexInputBindingDescription{
        /*binding=*/0,
        /*stride=*/sizeof(VertexAttrib3D),
        // for instancing, use _INSTANCE for .inputRate
        /*inputRate=*/VK_VERTEX_INPUT_RATE_VERTEX,
    },
};

const vector<VkVertexInputAttributeDescription> attrib_descs{
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

Descriptor::ImageInfos CreateImageInfos(
    const Model::Mesh& mesh,
    const Descriptor::Info& descriptor_info) {
  Descriptor::ImageInfos image_infos{};
  for (const auto& binding : descriptor_info.bindings) {
    const vector<TextureImage>& images = mesh[binding.texture_type];
    vector<VkDescriptorImageInfo> descriptor_infos{};
    descriptor_infos.reserve(images.size());
    for (const auto& image : images) {
      descriptor_infos.emplace_back(image.descriptor_info());
    }
    image_infos[binding.binding_point] = move(descriptor_infos);
  }
  return image_infos;
}

} /* namespace */

void Model::Init(SharedContext context,
                 unsigned int obj_index_base,
                 const string& obj_path,
                 const TextureBindingMap& binding_map,
                 const vector<UniformInfo>& uniform_infos,
                 const vector<Pipeline::ShaderInfo>& shader_infos,
                 size_t num_frame) {
  if (!is_first_time) {
    for (auto& drawables_per_frame : drawables_) {
      for (auto& drawable : drawables_per_frame) {
        drawable->Init();
      }
    }
    return;
  } else {
    is_first_time = false;
  }

  shader_infos_ = shader_infos;

  // load vertices and indices
  vector<VertexAttrib3D> vertices;
  vector<uint32_t> indices;
  common::util::LoadObjFromFile(obj_path, obj_index_base, &vertices, &indices);
  vertex_buffer_.Init(context, {CreateVertexInfo(vertices, indices)});

  // load textures
  const size_t max_num_sampler = static_cast<size_t>(
      context->physical_device().limits().maxPerStageDescriptorSamplers);
  size_t num_texture = 0;
  for (const auto& binding : binding_map) {
    num_texture += binding.second.texture_paths.size();
  }
  if (num_texture > max_num_sampler) {
    throw std::runtime_error{"We have " + std::to_string(num_texture) +
                             " textures to bind, but the maximal number of "
                             "samplers is " + std::to_string(max_num_sampler)};
  }

  Descriptor::Info texture_info{
      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
      /*bindings=*/{},  // will be populated later
  };
  meshes_.resize(1);
  for (const auto& binding : binding_map) {
    const TextureType type = binding.first;
    const vector<vector<string>>& texture_paths = binding.second.texture_paths;

    texture_info.bindings.emplace_back(Descriptor::Info::Binding{
        type, binding.second.binding_point, CONTAINER_SIZE(texture_paths),
    });
    meshes_[0][type].resize(texture_paths.size());
    for (size_t i = 0; i < texture_paths.size(); ++i) {
      meshes_[0][type][i].Init(context, texture_paths[i]);
    }
  }

  // create drawables
  const auto image_infos = CreateImageInfos(meshes_[0], texture_info);
  vector<Descriptor::Info> descriptor_infos{texture_info};
  for (const auto& uniform_info : uniform_infos) {
    descriptor_infos.emplace_back(*uniform_info.second);
  }
  drawables_.resize(num_frame);
  for (size_t frame = 0; frame < num_frame; ++frame) {
    std::unique_ptr<Descriptor> descriptor{new Descriptor};
    descriptor->Init(context, descriptor_infos);
    for (const auto& uniform_info : uniform_infos) {
      descriptor->UpdateBufferInfos(
          *uniform_info.second, {uniform_info.first->descriptor_info(frame)});
    }
    descriptor->UpdateImageInfos(texture_info.descriptor_type, image_infos);

    drawables_[frame].emplace_back(new Drawable{
        context, *this, /*start_index=*/0, /*end_index=*/1, move(descriptor)});
  }
}

void Model::Init(SharedContext context,
                 const vector<Pipeline::ShaderInfo>& shader_infos,
                 const string& obj_path,
                 const string& tex_path,
                 const BindingMap& bindings,
                 const vector<UniformInfo>& uniform_infos,
                 size_t num_frame) {
//  // load vertices and indices
//  common::ModelLoader loader{obj_path, tex_path, /*flip_uvs=*/false};
//  vector<VertexBuffer::Info> vertex_infos;
//  vertex_infos.reserve(loader.meshes().size());
//  for (const auto& mesh : loader.meshes()) {
//    vertex_infos.emplace_back(CreateVertexInfo(mesh.vertices, mesh.indices));
//  }
//  vertex_buffer_.Init(context, vertex_infos);
//
//  // load textures
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
//
//  vector<Descriptor::Info::Binding> texture_bindings;
//  for (int type = 0; type < TextureType::kTypeMaxEnum; ++type) {
//    if (!meshes_[0].textures[type].empty()) {
//      const uint32_t binding_point =
//          bindings.find(static_cast<TextureType>(type))->second.binding_point;
//      texture_bindings.emplace_back(
//        static_cast<TextureType>(type), binding_point,
//        CONTAINER_SIZE(meshes_[0].textures[type]));
//    }
//  }
//
//  // create descriptors
//  Descriptor::Info texture_info{
//      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//      /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
//      move(texture_bindings),
//  };
//  vector<Descriptor::Info> descriptor_infos{texture_info};
//  for (const auto& uniform_info : uniform_infos) {
//    descriptor_infos.emplace_back(*uniform_info.second);
//  }
//
//  descriptors_.resize(num_frame);
//  for (size_t frame = 0; frame < num_frame; ++frame) {
//    descriptors_[frame].resize(meshes_.size());
//  }
//  for (size_t i = 0; i < meshes_.size(); ++i) {
//    const auto image_infos = CreateImageInfos(meshes_[i], texture_info);
//    for (size_t frame = 0; frame < num_frame; ++frame) {
//      descriptors_[frame][i].Init(context, descriptor_infos);
//      for (const auto& uniform_info : uniform_infos) {
//        descriptors_[frame][i].UpdateBufferInfos(
//          *uniform_info.second, {uniform_info.first->descriptor_info(frame)});
//      }
//      descriptors_[frame][i].UpdateImageInfos(
//          texture_info.descriptor_type, image_infos);
//    }
//  }
}

void Model::Cleanup() {
  for (auto& drawables_per_frame : drawables_) {
    for (auto& drawable : drawables_per_frame) {
      drawable->Cleanup();
    }
  }
}

void Model::Draw(const VkCommandBuffer& command_buffer,
                 size_t frame) const {
  for (const auto& drawable : drawables_[frame]) {
    drawable->Draw(command_buffer);
  }
}

Model::Drawable::Drawable(const SharedContext& context,
                          const Model& model,
                          size_t start_index,
                          size_t end_index,
                          std::unique_ptr<Descriptor>&& descriptor)
    : context_{context}, model_{model}, start_index_{start_index},
      end_index_{end_index}, descriptor_{move(descriptor)} {
  Init();
}

void Model::Drawable::Init() {
  pipeline_.Init(context_, model_.shader_infos_, {descriptor_->layout()},
                 binding_descs, attrib_descs);
}

void Model::Drawable::Cleanup() {
  pipeline_.Cleanup();
}

void Model::Drawable::Draw(const VkCommandBuffer& command_buffer) const {
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    *pipeline_);
  for (size_t index = start_index_; index < end_index_; ++index) {
    vkCmdBindDescriptorSets(
        command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.layout(),
        /*firstSet=*/0, /*descriptorSetCount=*/1, &descriptor_->set(),
        /*dynamicOffsetCount=*/0, /*pDynamicOffsets=*/nullptr);
    model_.vertex_buffer_.Draw(command_buffer, index);
  }
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
