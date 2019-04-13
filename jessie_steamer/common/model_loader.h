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
    enum class Type { kDiffuse, kSpecular, kReflection };
    std::unique_ptr<util::Image> image;
    Type type;
  };

  struct Mesh {
    std::vector<util::VertexAttrib3D> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
  };

  ModelLoader(const std::string& obj_path,
              const std::string& tex_path,
              bool flip_uvs);

  // This class is neither copyable nor movable
  ModelLoader(const ModelLoader&) = delete;
  ModelLoader& operator=(const ModelLoader&) = delete;

  const std::vector<Mesh>& meshes() const { return meshes_; }

 private:
  std::vector<Mesh> meshes_;

  void ProcessNode(const std::string& directory,
                   const aiNode* node,
                   const aiScene* scene);
  Mesh ProcessMesh(const std::string& directory,
                   const aiMesh* mesh,
                   const aiScene* scene);
  std::vector<Texture> LoadTextures(const std::string& directory,
                                    const aiMaterial* material,
                                    ModelLoader::Texture::Type type);
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif //JESSIE_STEAMER_COMMON_MODEL_LOADER_H
