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

}

ModelLoader::ModelLoader(const string& obj_path,
                         const string& tex_path,
                         bool flip_uvs) {
  // other useful options:
  // - aiProcess_GenNormals: create normal for vertices
  // - aiProcess_SplitLargeMeshes: split mesh when the number of triangles
  //                               that can be rendered at a time is limited
  // - aiProcess_OptimizeMeshes: do the reverse of splitting, merge meshes to
  //                             reduce drawing calls
  unsigned int flags = aiProcess_Triangulate;
  flags |= flip_uvs ? aiProcess_FlipUVs : 0;

  Assimp::Importer importer;
  const auto* scene = importer.ReadFile(obj_path.c_str(), flags);
  if (scene == nullptr || scene->mRootNode ||
      scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
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
    meshes_.emplace_back(ProcessMesh(directory, mesh, scene));
  }
  for (int i = 0; i < node->mNumChildren; ++i) {
    ProcessNode(directory, node->mChildren[i], scene);
  }
}

ModelLoader::Mesh ModelLoader::ProcessMesh(const string& directory,
                                           const aiMesh* mesh,
                                           const aiScene* scene) {
  // load vertices
  vector<util::VertexAttrib3D> vertices;
  vertices.reserve(mesh->mNumVertices);
  aiVector3D* ai_tex_coords = mesh->mTextureCoords[0];
  for (int i = 0; i < vertices.size(); ++i) {
    glm::vec3 position{mesh->mVertices[i].x,
                       mesh->mVertices[i].y,
                       mesh->mVertices[i].z};
    glm::vec3 normal{mesh->mNormals[i].x,
                     mesh->mNormals[i].y,
                     mesh->mNormals[i].z};
    glm::vec2 tex_coord{ai_tex_coords ? ai_tex_coords[i].x : 0.0,
                        ai_tex_coords ? ai_tex_coords[i].y : 0.0};
    vertices[i] = util::VertexAttrib3D{position, normal, tex_coord};
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
     directory, material, Texture::Type::kDiffuse);
    vector<Texture> spec_maps = LoadTextures(
     directory, material, Texture::Type::kSpecular);
    vector<Texture> refl_maps = LoadTextures(
     directory, material, Texture::Type::kReflection);

    util::MoveAll(&textures, &diff_maps);
    util::MoveAll(&textures, &spec_maps);
    util::MoveAll(&textures, &refl_maps);
  }

  return Mesh{std::move(vertices), std::move(indices), std::move(textures)};
}

vector<ModelLoader::Texture> ModelLoader::LoadTextures(
    const string& directory,
    const aiMaterial* material,
    ModelLoader::Texture::Type type) {
  aiTextureType ai_type;
  switch (type) {
    case Texture::Type::kDiffuse:
      ai_type = aiTextureType_DIFFUSE;
      break;
    case Texture::Type::kSpecular:
      ai_type = aiTextureType_SPECULAR;
      break;
    case Texture::Type::kReflection:
      ai_type = aiTextureType_AMBIENT;
      break;
  }

  vector<Texture> textures(material->GetTextureCount(ai_type));
  for (int i = 0; i < textures.size(); ++i) {
    aiString path;
    material->GetTexture(ai_type, i, &path);
    std::unique_ptr<util::Image> image{
        new util::Image{directory + "/" + path.C_Str()}};
    textures[i] = Texture{std::move(image), type};
  }
  return textures;
}

} /* namespace common */
} /* namespace jessie_steamer */
