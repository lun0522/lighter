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

using std::string;
using std::vector;

} /* namespace */

ModelLoader::ModelLoader(const string& obj_path,
                         const string& tex_path) {
  // other useful options:
  // - aiProcess_GenNormals: create normal for vertices
  // - aiProcess_SplitLargeMeshes: split mesh when the number of triangles
  //                               that can be rendered at a time is limited
  // - aiProcess_OptimizeMeshes: do the reverse of splitting, merge meshes to
  //                             reduce drawing calls
  unsigned int flags = aiProcess_Triangulate
                           | aiProcess_GenNormals
                           | aiProcess_PreTransformVertices
                           | aiProcess_FlipUVs;

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
  meshes_.emplace_back();

  // load vertices
  vector<util::VertexAttrib3D>& vertices = meshes_.back().vertices;
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
  vector<unsigned int>& indices = meshes_.back().indices;
  for (int i = 0; i < mesh->mNumFaces; ++i) {
    aiFace face = mesh->mFaces[i];
    indices.insert(indices.end(), face.mIndices,
                   face.mIndices + face.mNumIndices);
  }

  // load textures
  vector<Texture>& textures = meshes_.back().textures;
  if (scene->HasMaterials()) {
    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    LoadTextures(directory, material, Texture::kTypeDiffuse, &textures);
    LoadTextures(directory, material, Texture::kTypeSpecular, &textures);
    LoadTextures(directory, material, Texture::kTypeReflection, &textures);
  }
}

void ModelLoader::LoadTextures(const string& directory,
                               const aiMaterial* material,
                               Texture::Type type,
                               vector<Texture>* textures) {
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
    default:
      throw std::runtime_error{"Unrecognized texture type"};
  }

  size_t num_texture = material->GetTextureCount(ai_type);
  textures->reserve(textures->size() + num_texture);
  for (int i = 0; i < num_texture; ++i) {
    aiString path;
    material->GetTexture(ai_type, i, &path);
    textures->emplace_back(Texture{directory + "/" + path.C_Str(), type});
  }
}

} /* namespace common */
} /* namespace jessie_steamer */
