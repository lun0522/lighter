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

// TODO: use absl::Span
Descriptor::ImageInfos CreateImageInfos(
    const vector<const Model::Mesh*>& meshes,
    const Descriptor::Info& descriptor_info) {
  Descriptor::ImageInfos image_infos{};
  for (const auto& binding : descriptor_info.bindings) {
    vector<VkDescriptorImageInfo> descriptor_infos{};
    descriptor_infos.reserve(meshes[0][binding.texture_type].size() *
                             meshes.size());
    for (const auto* mesh : meshes) {
      const vector<TextureImage>& images = mesh->at(binding.texture_type);
      for (const auto& image : images) {
        descriptor_infos.emplace_back(image.descriptor_info());
      }
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
    for (auto& drawable : drawables_) {
      drawable->Init();
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
  const auto image_infos = CreateImageInfos({meshes_.data()}, texture_info);
  vector<Descriptor::Info> descriptor_infos{texture_info};
  for (const auto& uniform_info : uniform_infos) {
    descriptor_infos.emplace_back(*uniform_info.second);
  }

  vector<unique_ptr<Descriptor>> descriptors;
  descriptors.reserve(num_frame);
  for (size_t frame = 0; frame < num_frame; ++frame) {
    descriptors.emplace_back(new Descriptor);
    descriptors.back()->Init(context, descriptor_infos);
    for (const auto& uniform_info : uniform_infos) {
      descriptors.back()->UpdateBufferInfos(
          *uniform_info.second, {uniform_info.first->descriptor_info(frame)});
    }
    descriptors.back()->UpdateImageInfos(
        texture_info.descriptor_type, image_infos);
  }

  drawables_.emplace_back(new Drawable{
      context, *this, /*range=*/{0, 1}, move(descriptors)});
}

void Model::Init(SharedContext context,
                 const string& obj_path,
                 const string& tex_path,
                 const BindingPointMap& bindings,
                 const vector<UniformInfo>& uniform_infos,
                 const vector<Pipeline::ShaderInfo>& shader_infos,
                 size_t num_frame) {
  if (!is_first_time) {
    for (auto& drawable : drawables_) {
      drawable->Init();
    }
    return;
  } else {
    is_first_time = false;
  }

  shader_infos_ = shader_infos;

  // load vertices and indices
  common::ModelLoader loader{obj_path, tex_path, /*flip_uvs=*/false};
  vector<VertexBuffer::Info> vertex_infos;
  vertex_infos.reserve(loader.meshes().size());
  for (const auto& mesh : loader.meshes()) {
    vertex_infos.emplace_back(CreateVertexInfo(mesh.vertices, mesh.indices));
  }
  vertex_buffer_.Init(context, vertex_infos);

  // load textures
  const size_t max_num_sampler = static_cast<size_t>(
      context->physical_device().limits().maxPerStageDescriptorSamplers);
  vector<Range> ranges = Drawable::GenRanges(loader.meshes(), max_num_sampler);

  meshes_.reserve(loader.meshes().size());
  for (auto& loaded_mesh : loader.meshes()) {
    meshes_.emplace_back();
    for (auto& loaded_tex : loaded_mesh.textures) {
      vector<common::util::Image> images;
      images.emplace_back(move(loaded_tex.image));
      meshes_.back()[loaded_tex.type].emplace_back();
      meshes_.back()[loaded_tex.type].back().Init(context, images);
    }
  }

  vector<Descriptor::Info::Binding> texture_bindings;
  for (int type = 0; type < TextureType::kTypeMaxEnum; ++type) {
    if (!meshes_[0][type].empty()) {
      const uint32_t binding_point =
          bindings.find(static_cast<TextureType>(type))->second;
      texture_bindings.emplace_back(Descriptor::Info::Binding{
          static_cast<TextureType>(type),
          binding_point,
          0,  // will be populated later
      });
    }
  }

  Descriptor::Info texture_info{
      /*descriptor_type=*/VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      /*shader_stage=*/VK_SHADER_STAGE_FRAGMENT_BIT,
      move(texture_bindings),
  };

  // create drawables
  vector<Descriptor::Info> descriptor_infos(uniform_infos.size() + 1);
  for (size_t i = 0; i < uniform_infos.size(); ++i) {
    descriptor_infos[i] = *uniform_infos[i].second;
  }
  drawables_.reserve(ranges.size());
  for (const auto& range : ranges) {
    const size_t num_mesh = range.end - range.start;
    for (auto& binding : texture_info.bindings) {
      binding.array_length = static_cast<uint32_t>(
          meshes_[0][binding.texture_type].size() * num_mesh);
    }
    descriptor_infos.back() = texture_info;

    vector<const Model::Mesh*> meshes(num_mesh);
    for (size_t index = range.start; index < range.end; ++index) {
      meshes[index - range.start] = &meshes_[index];
    }
    const auto image_infos = CreateImageInfos(meshes, texture_info);

    vector<unique_ptr<Descriptor>> descriptors;
    descriptors.reserve(num_frame);
    for (size_t frame = 0; frame < num_frame; ++frame) {
      descriptors.emplace_back(new Descriptor);
      descriptors.back()->Init(context, descriptor_infos);
      for (const auto& uniform_info : uniform_infos) {
        descriptors.back()->UpdateBufferInfos(
            *uniform_info.second, {uniform_info.first->descriptor_info(frame)});
      }
      descriptors.back()->UpdateImageInfos(
          texture_info.descriptor_type, image_infos);
    }
    drawables_.emplace_back(new Drawable{
        context, *this, range, move(descriptors)});
  }
}

void Model::Cleanup() {
  for (auto& drawable : drawables_) {
    drawable->Cleanup();
  }
}

void Model::Draw(const VkCommandBuffer& command_buffer,
                 size_t frame) const {
  for (const auto& drawable : drawables_) {
    drawable->Draw(command_buffer, frame);
  }
}

Model::Drawable::Drawable(const SharedContext& context,
                          const Model& model,
                          Range range,
                          vector<unique_ptr<Descriptor>>&& descriptors)
    : context_{context}, model_{model}, range_{range},
      descriptors_{move(descriptors)} {
  Init();
}

void Model::Drawable::Init() {
  pipeline_.Init(context_, model_.shader_infos_, {descriptors_[0]->layout()},
                 binding_descs, attrib_descs);
}

void Model::Drawable::Cleanup() {
  pipeline_.Cleanup();
}

void Model::Drawable::Draw(const VkCommandBuffer& command_buffer,
                           size_t frame) const {
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    *pipeline_);
  for (size_t index = range_.start; index < range_.end; ++index) {
    vkCmdBindDescriptorSets(
        command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.layout(),
        /*firstSet=*/0, /*descriptorSetCount=*/1, &descriptors_[frame]->set(),
        /*dynamicOffsetCount=*/0, /*pDynamicOffsets=*/nullptr);
    model_.vertex_buffer_.Draw(command_buffer, index);
  }
}

vector<Model::Range> Model::Drawable::GenRanges(
  const vector<common::ModelLoader::Mesh>& meshes,
  size_t max_num_sampler) {
  const size_t num_texture_per_mesh = meshes[0].textures.size();
  if (num_texture_per_mesh > max_num_sampler) {
    throw std::runtime_error{
        "Each mesh contains " + std::to_string(num_texture_per_mesh) +
        " textures, but we can only have " + std::to_string(max_num_sampler) +
        " samplers per-stage"};
  }

  const size_t num_mesh = meshes.size();
  const size_t total_num_texture = num_texture_per_mesh * num_mesh;
  const size_t num_range = (total_num_texture + max_num_sampler - 1) /
                           max_num_sampler;
  vector<Model::Range> ranges;
  ranges.reserve(num_range);

#ifdef DEBUG
  if (total_num_texture > max_num_sampler) {
    std::cout << "We have " << std::to_string(total_num_texture)
              << " textures to bind, but the maximal number of samplers is "
              << std::to_string(max_num_sampler)
              << ". This model will be rendered in "
              << std::to_string(num_range) << " passes" << std::endl;
  }
#endif

  const size_t num_mesh_per_drawable = max_num_sampler / num_texture_per_mesh;
  for (size_t start = 0; start < num_mesh; start += num_mesh_per_drawable) {
    ranges.emplace_back(Range{start, std::min(start + num_mesh_per_drawable,
                                              num_mesh)});
  }
  return ranges;
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
