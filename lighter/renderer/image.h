//
//  image.h
//
//  Created by Pujun Lun on 10/19/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IMAGE_H
#define LIGHTER_RENDERER_IMAGE_H

#include <string>
#include <string_view>

#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/type.h"

namespace lighter::renderer {

class DeviceImage {
 public:
  // This class is neither copyable nor movable.
  DeviceImage(const DeviceImage&) = delete;
  DeviceImage& operator=(const DeviceImage&) = delete;

  virtual ~DeviceImage() = default;

  // Accessors.
  const std::string& name() const { return name_; }
  SampleCount sample_count() const { return sample_count_; }

 protected:
  DeviceImage(std::string_view name, SampleCount sample_count)
      : name_{name}, sample_count_{sample_count} {}

 private:
  const std::string name_;

  const SampleCount sample_count_;
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

}  // namespace lighter::renderer

#endif  // LIGHTER_RENDERER_IMAGE_H
