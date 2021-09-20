//
//  property_checker.cc
//
//  Created by Pujun Lun on 6/11/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/property_checker.h"

#include <string_view>

#include "lighter/common/util.h"

namespace lighter::renderer::vk {
namespace {

// Helpers function to retrieve the property name.
template <typename PropType>
std::string GetPropertyName(const PropType& properties);

template <>
std::string GetPropertyName<intl::LayerProperties>(
    const intl::LayerProperties& properties) {
  return properties.layerName;
}

template <>
std::string GetPropertyName<intl::ExtensionProperties>(
    const intl::ExtensionProperties& properties) {
  return properties.extensionName;
}

// Extracts the names of properties and pours them into a hash set.
template <typename PropType>
absl::flat_hash_set<std::string> GetPropertyNamesSet(
    absl::Span<const PropType> properties) {
  return common::util::TransformToSet<PropType, std::string>(
      properties, GetPropertyName<PropType>);
}

// Prints in the format:
// <header>
// \t<elem1>
// \t<elem2>
// ...
template <typename ForwardIterator>
void PrintElements(std::string_view header, ForwardIterator begin,
                   ForwardIterator end) {
  LOG_INFO << header;
  for (ForwardIterator iter = begin; iter != end; ++iter) {
    LOG_INFO << "\t" << *iter;
  }
}

// Convenient function to print elements in 'container'.
void PrintElements(std::string_view header,
                   absl::Span<const std::string> container) {
  PrintElements(header, container.begin(), container.end());
}

}  // namespace

PropertyChecker PropertyChecker::ForInstanceLayers() {
  return PropertyChecker{GetPropertyNamesSet<intl::LayerProperties>(
      intl::enumerateInstanceLayerProperties())};
}

PropertyChecker PropertyChecker::ForInstanceExtensions() {
  return PropertyChecker{GetPropertyNamesSet<intl::ExtensionProperties>(
      intl::enumerateInstanceExtensionProperties())};
}

PropertyChecker PropertyChecker::ForDeviceLayers(
    intl::PhysicalDevice physical_device) {
  return PropertyChecker{GetPropertyNamesSet<intl::LayerProperties>(
      physical_device.enumerateDeviceLayerProperties())};
}

PropertyChecker PropertyChecker::ForDeviceExtensions(
    intl::PhysicalDevice physical_device) {
  return PropertyChecker{GetPropertyNamesSet<intl::ExtensionProperties>(
      physical_device.enumerateDeviceExtensionProperties())};
}

std::vector<std::string> PropertyChecker::FindUnsupported(
    absl::Span<const std::string> required_properties) const {
  if (required_properties.empty()) {
    LOG_INFO << "No property requested, skip";
    LOG_INFO;
    return {};
  }

  PrintElements("Supported:", supported_properties_.begin(),
                supported_properties_.end());
  PrintElements("Required:", required_properties);

  auto unsupported_properties = common::util::CopyToVectorIf<std::string>(
      required_properties, [this](const std::string& property) {
        return !supported_properties_.contains(property);
      });
  if (unsupported_properties.empty()) {
    LOG_INFO << "All supported";
  } else {
    PrintElements("Unsupported:", unsupported_properties);
  }
  LOG_INFO;

  return unsupported_properties;
}

}  // namespace lighter::renderer::vk
