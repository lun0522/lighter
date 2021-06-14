//
//  property_checker.h
//
//  Created by Pujun Lun on 6/11/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_PROPERTY_CHECKER_H
#define LIGHTER_RENDERER_VK_PROPERTY_CHECKER_H

#include <string>
#include <vector>

#include "lighter/renderer/vk/util.h"
#include "third_party/absl/container/flat_hash_set.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer::vk {

// Helper class to query Vulkan instance/device layers/extensions and check if
// our required properties are supported.
class PropertyChecker {
 public:
  static PropertyChecker ForInstanceLayers();
  static PropertyChecker ForInstanceExtensions();
  static PropertyChecker ForDeviceLayers(intl::PhysicalDevice physical_device);
  static PropertyChecker ForDeviceExtensions(
      intl::PhysicalDevice physical_device);

  // This class is only movable.
  PropertyChecker(PropertyChecker&&) noexcept = default;
  PropertyChecker& operator=(PropertyChecker&&) noexcept = default;

  // Returns true if the property is supported.
  bool IsSupported(const std::string& required_property) const {
    return supported_properties_.contains(required_property);
  }

  // Returns a vector of unsupported properties if exists any. It also prints
  // out supported, required and unsupported (if exists any) properties.
  std::vector<std::string> FindUnsupported(
      absl::Span<const std::string> required_properties) const;

  // Returns true if all properties are supported. It also prints out properties
  // as FindUnsupported().
  bool AreSupported(absl::Span<const std::string> required_properties) const {
    return FindUnsupported(required_properties).empty();
  }

 private:
  PropertyChecker(absl::flat_hash_set<std::string>&& supported_properties)
      : supported_properties_{std::move(supported_properties)} {}

  // Holds all supported properties.
  absl::flat_hash_set<std::string> supported_properties_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_PROPERTY_CHECKER_H
