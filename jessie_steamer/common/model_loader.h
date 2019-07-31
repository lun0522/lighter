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

enum class ResourceType {
  kTextureDiffuse = 0,
  kTextureSpecular,
  kTextureReflection,
  kTextureCubemap,
  kNumTextureType,
  kUniformBuffer,
};

class ModelLoader {
 public:
  struct Texture {
    // This class is only movable.
    Texture(Texture&&) noexcept = default;
    Texture& operator=(Texture&&) noexcept = default;

    std::string path;
    ResourceType resource_type;
  };

  struct Mesh {
    Mesh() = default;

    // This class is only movable.
    Mesh(Mesh&&) noexcept = default;
    Mesh& operator=(Mesh&&) noexcept = default;

    std::vector<VertexAttrib3D> vertices;
    std::vector<uint32_t> indices;
    std::vector<Texture> textures;
  };

  ModelLoader(const std::string& obj_path, const std::string& tex_path);

  // This class is neither copyable nor movable.
  ModelLoader(const ModelLoader&) = delete;
  ModelLoader& operator=(const ModelLoader&) = delete;

  const std::vector<Mesh>& meshes() const { return meshes_; }

 private:
  void ProcessNode(const std::string& directory,
                   const aiNode* node,
                   const aiScene* scene);
  void ProcessMesh(const std::string& directory,
                   const aiMesh* mesh,
                   const aiScene* scene);
  void LoadTextures(const std::string& directory,
                    const aiMaterial* material,
                    ResourceType resource_type,
                    std::vector<Texture>* textures);

  std::vector<Mesh> meshes_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_MODEL_LOADER_H */
