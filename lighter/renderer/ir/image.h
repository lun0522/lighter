//
//  image.h
//
//  Created by Pujun Lun on 10/19/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IR_IMAGE_H
#define LIGHTER_RENDERER_IR_IMAGE_H

#include <glm/glm.hpp>
#include <string>
#include <string_view>

#include "lighter/base/image.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::renderer::ir {

class Image {
 public:
  using LayerType = base::image::Type;

  // This class is neither copyable nor movable.
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  virtual ~Image() = default;

  int GetNumLayers() const { return base::image::GetNumLayers(layer_type_); }

  // Accessors.
  const std::string& name() const { return name_; }
  LayerType layer_type() const { return layer_type_; }
  int width() const { return extent_.x; }
  int height() const { return extent_.y; }
  int mip_levels() const { return mip_levels_; }

 protected:
  Image(std::string_view name, LayerType layer_type, const glm::ivec2& extent,
        int mip_levels)
      : name_{name},
        layer_type_{layer_type},
        extent_{extent},
        mip_levels_{mip_levels} {}

 private:
  const std::string name_;
  const LayerType layer_type_;
  const glm::ivec2 extent_;
  const int mip_levels_;
};

struct SamplerDescriptor {
  FilterType filter_type;
  SamplerAddressMode address_mode;
};

}  // namespace lighter::renderer::ir

#endif  // LIGHTER_RENDERER_IR_IMAGE_H
