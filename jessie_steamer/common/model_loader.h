//
//  model_loader.h
//
//  Created by Pujun Lun on 4/13/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_MODEL_LOADER_H
#define JESSIE_STEAMER_COMMON_MODEL_LOADER_H

#include <memory>
#include <vector>

#include "absl/memory/memory.h"
#include "jessie_steamer/common/util.h"

struct aiMaterial;
struct aiMesh;
struct aiNode;
struct aiScene;

namespace jessie_steamer {
namespace common {

class ModelLoader {
 public:
  struct Texture {
    enum Type {
      kTypeDiffuse = 0,
      kTypeSpecular,
      kTypeReflection,
      kTypeMaxEnum,
    };

    Texture(const std::string& path, Type type)
        : image{absl::make_unique<util::Image>(path)}, type{type} {}

    // This class is neither copyable nor movable
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    std::unique_ptr<util::Image> image;
    Type type;
  };

  struct Mesh {
    Mesh() = default;

    // This class is neither copyable nor movable
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    std::vector<util::VertexAttrib3D> vertices;
    std::vector<unsigned int> indices;
    std::vector<std::unique_ptr<Texture>> textures;
  };

  ModelLoader(const std::string& obj_path,
              const std::string& tex_path,
              bool is_left_handed);

  // This class is neither copyable nor movable
  ModelLoader(const ModelLoader&) = delete;
  ModelLoader& operator=(const ModelLoader&) = delete;

  std::vector<std::unique_ptr<Mesh>>& meshes() { return meshes_; }

 private:
  void ProcessNode(const std::string& directory,
                   const aiNode* node,
                   const aiScene* scene);
  void ProcessMesh(const std::string& directory,
                   const aiMesh* mesh,
                   const aiScene* scene);
  void LoadTextures(const std::string& directory,
                    const aiMaterial* material,
                    ModelLoader::Texture::Type type,
                    std::vector<std::unique_ptr<Texture>>* textures);

  std::vector<std::unique_ptr<Mesh>> meshes_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif //JESSIE_STEAMER_COMMON_MODEL_LOADER_H
