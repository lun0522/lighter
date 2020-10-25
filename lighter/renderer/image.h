//
//  image.h
//
//  Created by Pujun Lun on 10/19/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IMAGE_H
#define LIGHTER_RENDERER_IMAGE_H

#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/type.h"

namespace lighter {
namespace renderer {

class DeviceImage {
 public:
  // This class is neither copyable nor movable.
  DeviceImage(const DeviceImage&) = delete;
  DeviceImage& operator=(const DeviceImage&) = delete;

  virtual ~DeviceImage() = default;

 protected:
  DeviceImage() = default;
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

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_IMAGE_H */
