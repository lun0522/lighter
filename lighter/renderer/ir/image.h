//
//  image.h
//
//  Created by Pujun Lun on 10/19/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IR_IMAGE_H
#define LIGHTER_RENDERER_IR_IMAGE_H

#include <algorithm>
#include <string>
#include <string_view>

#include "lighter/common/image.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/type.h"
#include "third_party/glm/glm.hpp"

namespace lighter::renderer::ir {

class DeviceImage {
 public:
  using Type = common::image::Type;

  // This class is neither copyable nor movable.
  DeviceImage(const DeviceImage&) = delete;
  DeviceImage& operator=(const DeviceImage&) = delete;

  virtual ~DeviceImage() = default;

  int GetNumLayers() const { return common::image::GetNumLayers(type_); }

  int CalculateMipLevels() const {
    const int largest_dim = std::max(width(), height());
    return std::floor(static_cast<float>(std::log2(largest_dim)));
  }

  // Accessors.
  const std::string& name() const { return name_; }
  Type type() const { return type_; }
  int width() const { return extent_.x; }
  int height() const { return extent_.y; }
  int mip_levels() const { return mip_levels_; }

 protected:
  DeviceImage(std::string_view name, Type type, const glm::ivec2& extent,
              int mip_levels)
      : name_{name}, type_{type}, extent_{extent}, mip_levels_{mip_levels} {}

 private:
  const std::string name_;
  const Type type_;
  const glm::ivec2 extent_;
  const int mip_levels_;
};

struct SamplerDescriptor {
  FilterType filter_type;
  SamplerAddressMode address_mode;
};

class SampledImageView {
 public:
  // This class provides copy constructor and move constructor.
  SampledImageView(SampledImageView&&) noexcept = default;
  SampledImageView(const SampledImageView&) = default;

  virtual ~SampledImageView() = default;

 protected:
  SampledImageView() = default;
};

}  // namespace lighter::renderer::ir

#endif  // LIGHTER_RENDERER_IR_IMAGE_H
