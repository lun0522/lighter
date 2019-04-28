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
    util::Image image;
    Type type;

    Texture(util::Image&& image, Type type)
        : image{std::move(image)}, type{type} {}

    // This class is only movable
    Texture(Texture&&) = default;
    Texture& operator=(Texture&&) = default;
  };

  struct Mesh {
    std::vector<util::VertexAttrib3D> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    Mesh(std::vector<util::VertexAttrib3D>&& vertices,
         std::vector<unsigned int>&& indices,
         std::vector<Texture>&& textures)
        : vertices{std::move(vertices)},
          indices{std::move(indices)},
          textures{std::move(textures)} {}

    // This class is only movable
    Mesh(Mesh&&) = default;
    Mesh& operator=(Mesh&&) = default;
  };

  ModelLoader(const std::string& obj_path,
              const std::string& tex_path,
              bool is_left_handed);

  // This class is neither copyable nor movable
  ModelLoader(const ModelLoader&) = delete;
  ModelLoader& operator=(const ModelLoader&) = delete;

  std::vector<Mesh>& meshes() { return meshes_; }

 private:
  std::vector<Mesh> meshes_;

  void ProcessNode(const std::string& directory,
                   const aiNode* node,
                   const aiScene* scene);
  void ProcessMesh(const std::string& directory,
                   const aiMesh* mesh,
                   const aiScene* scene);
  std::vector<Texture> LoadTextures(const std::string& directory,
                                    const aiMaterial* material,
                                    ModelLoader::Texture::Type type);
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif //JESSIE_STEAMER_COMMON_MODEL_LOADER_H
