//
//  model_loader.h
//
//  Created by Pujun Lun on 4/13/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_MODEL_LOADER_H
#define JESSIE_STEAMER_COMMON_MODEL_LOADER_H

#include <string>
#include <vector>

#include "jessie_steamer/common/file.h"
#include "third_party/assimp/material.h"
#include "third_party/assimp/mesh.h"
#include "third_party/assimp/scene.h"

namespace jessie_steamer {
namespace common {

// Texture types that can be bound to shaders.
enum class TextureType { kDiffuse, kSpecular, kReflection, kCubemap, kNumType };

// Model loader backed by Assimp.
class ModelLoader {
 public:
  // Information about a texture.
  struct TextureInfo {
    // This class is only movable.
    TextureInfo(TextureInfo&&) noexcept = default;
    TextureInfo& operator=(TextureInfo&&) noexcept = default;

    // Path to the texture.
    std::string path;

    // Texture type.
    TextureType texture_type;
  };

  // Vertex data and textures information for one mesh.
  struct MeshData {
    MeshData() = default;

    // This class is only movable.
    MeshData(MeshData&&) noexcept = default;
    MeshData& operator=(MeshData&&) noexcept = default;

    // Vertex data of the mesh.
    std::vector<VertexAttrib3D> vertices;
    std::vector<uint32_t> indices;

    // Textures information of the mesh.
    std::vector<TextureInfo> textures;
  };

  // Loads a Wavefront .obj file from 'obj_path' and textures from the directory
  // 'tex_path'.
  ModelLoader(const std::string& obj_path, const std::string& tex_path);

  // This class is neither copyable nor movable.
  ModelLoader(const ModelLoader&) = delete;
  ModelLoader& operator=(const ModelLoader&) = delete;

  // Accessors.
  const std::vector<MeshData>& mesh_datas() const { return mesh_datas_; }

 private:
  // Processes the 'node' in Assimp scene graph. This adds all the data of
  // meshes stored in 'node' to 'mesh_datas_', and recursively processes all
  // children nodes.
  void ProcessNode(const std::string& directory,
                   const aiNode* node,
                   const aiScene* scene);

  // Loads mesh data from the given 'mesh'.
  MeshData LoadMesh(const std::string& directory,
                    const aiMesh* mesh,
                    const aiScene* scene) const;

  // Loads textures of the given 'texture_type' and appends to 'texture_infos'.
  void LoadTextures(const std::string& directory,
                    const aiMaterial* material,
                    TextureType texture_type,
                    std::vector<TextureInfo>* texture_infos) const;

  // Holds the data of all meshes in one model.
  std::vector<MeshData> mesh_datas_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_MODEL_LOADER_H */
