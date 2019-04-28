//
//  model_loader.cc
//
//  Created by Pujun Lun on 4/13/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/model_loader.h"

#include <stdexcept>

#include "third_party/assimp/Importer.hpp"
#include "third_party/assimp/material.h"
#include "third_party/assimp/mesh.h"
#include "third_party/assimp/postprocess.h"
#include "third_party/assimp/scene.h"

namespace jessie_steamer {
namespace common {
namespace {

using std::move;
using std::string;
using std::vector;

} /* namespace */

ModelLoader::ModelLoader(const string& obj_path,
                         const string& tex_path,
                         bool is_left_handed) {
  // other useful options:
  // - aiProcess_GenNormals: create normal for vertices
  // - aiProcess_SplitLargeMeshes: split mesh when the number of triangles
  //                               that can be rendered at a time is limited
  // - aiProcess_OptimizeMeshes: do the reverse of splitting, merge meshes to
  //                             reduce drawing calls
  unsigned int flags = aiProcess_Triangulate | aiProcess_GenNormals;
  flags |= is_left_handed ? aiProcess_ConvertToLeftHanded : 0;

  Assimp::Importer importer;
   auto* scene = importer.ReadFile(obj_path, flags);
  if (!scene || !scene->mRootNode ||
      (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
    throw std::runtime_error{string{"Failed to import scene: "} +
                             importer.GetErrorString()};
  }

  ProcessNode(tex_path, scene->mRootNode, scene);
}

void ModelLoader::ProcessNode(const string& directory,
                              const aiNode* node,
                              const aiScene* scene) {
  for (int i = 0; i < node->mNumMeshes; ++i) {
    aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
    ProcessMesh(directory, mesh, scene);
  }
  for (int i = 0; i < node->mNumChildren; ++i) {
    ProcessNode(directory, node->mChildren[i], scene);
  }
}

void ModelLoader::ProcessMesh(const string& directory,
                              const aiMesh* mesh,
                              const aiScene* scene) {
  // load vertices
  vector<util::VertexAttrib3D> vertices;
  vertices.reserve(mesh->mNumVertices);
  aiVector3D* ai_tex_coords = mesh->mTextureCoords[0];
  for (int i = 0; i < mesh->mNumVertices; ++i) {
    glm::vec3 position{mesh->mVertices[i].x,
                       mesh->mVertices[i].y,
                       mesh->mVertices[i].z};
    glm::vec3 normal{mesh->mNormals[i].x,
                     mesh->mNormals[i].y,
                     mesh->mNormals[i].z};
    glm::vec2 tex_coord{ai_tex_coords ? ai_tex_coords[i].x : 0.0,
                        ai_tex_coords ? ai_tex_coords[i].y : 0.0};
    vertices.emplace_back(position, normal, tex_coord);
  }

  // load indices
  vector<unsigned int> indices;
  for (int i = 0; i < mesh->mNumFaces; ++i) {
    aiFace face = mesh->mFaces[i];
    indices.insert(indices.end(), face.mIndices,
                   face.mIndices + face.mNumIndices);
  }

  // load textures
  vector<Texture> textures;
  if (scene->HasMaterials()) {
    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    vector<Texture> diff_maps = LoadTextures(
        directory, material, Texture::kTypeDiffuse);
    vector<Texture> spec_maps = LoadTextures(
        directory, material, Texture::kTypeSpecular);
    vector<Texture> refl_maps = LoadTextures(
        directory, material, Texture::kTypeReflection);

    util::MoveAll(&textures, &diff_maps);
    util::MoveAll(&textures, &spec_maps);
    util::MoveAll(&textures, &refl_maps);
  }

  meshes_.emplace_back(move(vertices), move(indices), move(textures));
}

vector<ModelLoader::Texture> ModelLoader::LoadTextures(
    const string& directory,
    const aiMaterial* material,
    Texture::Type type) {
  aiTextureType ai_type;
  switch (type) {
    case Texture::kTypeDiffuse:
      ai_type = aiTextureType_DIFFUSE;
      break;
    case Texture::kTypeSpecular:
      ai_type = aiTextureType_SPECULAR;
      break;
    case Texture::kTypeReflection:
      ai_type = aiTextureType_AMBIENT;
      break;
    case Texture::kTypeMaxEnum:
      throw std::runtime_error{"Invalid texture type"};
  }

  size_t num_texture = material->GetTextureCount(ai_type);
  vector<Texture> textures;
  textures.reserve(num_texture);
  for (int i = 0; i < num_texture; ++i) {
    aiString path;
    material->GetTexture(ai_type, i, &path);
    util::Image image{directory + "/" + path.C_Str()};
    textures.emplace_back(move(image), type);
  }
  return textures;
}

} /* namespace common */
} /* namespace jessie_steamer */
