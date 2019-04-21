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

void CreateTextureInfos(const vector<const Model::Mesh*>& meshes,
                        const Model::FindBindingPoint& find_binding_point,
                        Descriptor::ImageInfos* image_infos,
                        Descriptor::Info* texture_info) {
  vector<Descriptor::Info::Binding> texture_bindings;

  for (int type = 0; type < Model::TextureType::kTypeMaxEnum; ++type) {
    const auto texture_type = static_cast<Model::TextureType>(type);
    if (!meshes[0]->at(texture_type).empty()) {
      const uint32_t binding_point = find_binding_point(texture_type);
      const size_t num_texture = meshes[0]->at(texture_type).size() *
                                 meshes.size();

      vector<VkDescriptorImageInfo> descriptor_infos{};
      descriptor_infos.reserve(num_texture);
      for (const auto* mesh : meshes) {
        const vector<TextureImage>& images = mesh->at(texture_type);
        for (const auto& image : images) {
          descriptor_infos.emplace_back(image.descriptor_info());
        }
      }
      image_infos->operator[](binding_point) = move(descriptor_infos);

      texture_bindings.emplace_back(Descriptor::Info::Binding{
          texture_type,
          binding_point,
          static_cast<uint32_t>(num_texture),
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

bool Model::RunInit() {
  if (is_first_time_) {
    is_first_time_ = false;
    return true;
  } else {
    for (auto& drawable : drawables_) {
      drawable->Init();
    }
    return false;
  }
}

void Model::Init(SharedContext context,
                 unsigned int obj_index_base,
                 const string& obj_path,
                 const TextureBindingMap& binding_map,
                 const vector<UniformInfo>& uniform_infos,
                 const vector<Pipeline::ShaderInfo>& shader_infos,
                 size_t num_frame) {
  if (!RunInit()) return;

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

  meshes_.emplace_back();
  for (const auto& binding : binding_map) {
    const TextureType type = binding.first;
    const vector<vector<string>>& texture_paths = binding.second.texture_paths;

    meshes_.back()[type].resize(texture_paths.size());
    for (size_t i = 0; i < texture_paths.size(); ++i) {
      meshes_.back()[type][i].Init(context, texture_paths[i]);
    }
  }

  CreateDrawables(context, {{/*start=*/0, /*end=*/1}}, uniform_infos, num_frame,
                  [&binding_map](TextureType type) {
    return binding_map.find(type)->second.binding_point;
  });
}

void Model::Init(SharedContext context,
                 const string& obj_path,
                 const string& tex_path,
                 const BindingPointMap& binding_map,
                 const vector<UniformInfo>& uniform_infos,
                 const vector<Pipeline::ShaderInfo>& shader_infos,
                 size_t num_frame) {
  if (!RunInit()) return;

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

  const size_t max_num_sampler = static_cast<size_t>(
      context->physical_device().limits().maxPerStageDescriptorSamplers);
  vector<Range> ranges = Drawable::GenRanges(loader.meshes(), max_num_sampler);

  CreateDrawables(context, ranges, uniform_infos, num_frame,
                  [&binding_map](TextureType type) {
    return binding_map.find(type)->second;
  });
}

void Model::Cleanup() {
  for (auto& drawable : drawables_) {
    drawable->Cleanup();
  }
}

void Model::CreateDrawables(const SharedContext& context,
                            const vector<Range>& ranges,
                            const vector<UniformInfo>& uniform_infos,
                            size_t num_frame,
                            const FindBindingPoint& find_binding_point) {
  drawables_.reserve(ranges.size());
  for (const auto& range : ranges) {
    // create texture info
    // TODO: use absl::Span
    vector<const Model::Mesh*> meshes(range.end - range.start);
    for (size_t index = range.start; index < range.end; ++index) {
      meshes[index - range.start] = &meshes_[index];
    }
    Descriptor::ImageInfos image_infos;
    Descriptor::Info texture_info;
    CreateTextureInfos(meshes, find_binding_point, &image_infos, &texture_info);
    const VkDescriptorType tex_desc_type = texture_info.descriptor_type;

    // create descriptor infos
    vector<Descriptor::Info> descriptor_infos{move(texture_info)};
    for (const auto& uniform_info : uniform_infos) {
      descriptor_infos.emplace_back(*uniform_info.second);
    }

    // create descriptors
    vector<unique_ptr<Descriptor>> descriptors;
    descriptors.reserve(num_frame);
    for (size_t frame = 0; frame < num_frame; ++frame) {
      descriptors.emplace_back(new Descriptor);
      descriptors.back()->Init(context, descriptor_infos);
      for (const auto& uniform_info : uniform_infos) {
        descriptors.back()->UpdateBufferInfos(
            *uniform_info.second, {uniform_info.first->descriptor_info(frame)});
      }
      descriptors.back()->UpdateImageInfos(tex_desc_type, image_infos);
    }

    drawables_.emplace_back(new Drawable{
        context, *this, range, move(descriptors)});
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
  // set vertices info
  VkPipelineVertexInputStateCreateInfo vertex_input_info{
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      /*pNext=*/nullptr,
      common::util::nullflag,
      // vertex binding descriptions
      CONTAINER_SIZE(binding_descs),
      binding_descs.data(),
      // vertex attribute descriptions
      CONTAINER_SIZE(attrib_descs),
      attrib_descs.data(),
  };
  // used to set uniform values
  VkPipelineLayoutCreateInfo layout_info{
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      /*pNext=*/nullptr,
      common::util::nullflag,
      /*setLayoutCount=*/1,
      &descriptors_[0]->layout(),
      /*pushConstantRangeCount=*/0,
      /*pPushConstantRanges=*/nullptr,
  };
  pipeline_.Init(context_, model_.shader_infos_, layout_info,
                 vertex_input_info);
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
        " samplers per-stage"
    };
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
