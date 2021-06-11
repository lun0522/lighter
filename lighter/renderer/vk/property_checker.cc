//
//  property_checker.cc
//
//  Created by Pujun Lun on 6/11/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/property_checker.h"

#include <string_view>

#include "lighter/common/util.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/functional/function_ref.h"

namespace lighter::renderer::vk {
namespace {

// Helpers function to retrieve the property name.
template <typename PropType>
std::string GetPropertyName(const PropType& properties);

template <>
std::string GetPropertyName<VkLayerProperties>(
    const VkLayerProperties& properties) {
  return properties.layerName;
}

template <>
std::string GetPropertyName<VkExtensionProperties>(
    const VkExtensionProperties& properties) {
  return properties.extensionName;
}

// Queries properties, extracts their names and pours into a hash set.
template <typename PropType>
absl::flat_hash_set<std::string> GetPropertyNamesSet(
    absl::FunctionRef<void(uint32_t*, PropType*)> enumerate) {
  const std::vector<PropType> properties = util::QueryAttribute(enumerate);
  return common::util::TransformToSet<PropType, std::string>(
      properties, GetPropertyName<PropType>);
}

// Prints in the format:
// <header>
// \t<elem1>
// \t<elem2>
// ...
// <newline>
template <typename ForwardIterator>
void PrintElements(std::string_view header, ForwardIterator begin,
                   ForwardIterator end) {
  LOG_INFO << header;
  for (ForwardIterator iter = begin; iter != end; ++iter) {
    LOG_INFO << "\t" << *iter;
  }
  LOG_EMPTY_LINE;
}

// Convenient function to print elements in 'container'.
void PrintElements(std::string_view header,
                   absl::Span<const std::string> container) {
  PrintElements(header, container.begin(), container.end());
}

}  // namespace

PropertyChecker PropertyChecker::ForInstanceLayers() {
  return PropertyChecker{GetPropertyNamesSet<VkLayerProperties>(
      [](uint32_t* count, auto* properties) {
        return vkEnumerateInstanceLayerProperties(count, properties);
      }
  )};
}

PropertyChecker PropertyChecker::ForInstanceExtensions() {
  return PropertyChecker{GetPropertyNamesSet<VkExtensionProperties>(
      [](uint32_t* count, auto* properties) {
        return vkEnumerateInstanceExtensionProperties(/*pLayerName=*/nullptr,
                                                      count, properties);
      }
  )};
}

PropertyChecker PropertyChecker::ForDeviceLayers(
    VkPhysicalDevice physical_device) {
  return PropertyChecker{GetPropertyNamesSet<VkLayerProperties>(
      [physical_device](uint32_t* count, auto* properties) {
        return vkEnumerateDeviceLayerProperties(physical_device, count,
                                                properties);
      }
  )};
}

PropertyChecker PropertyChecker::ForDeviceExtensions(
    VkPhysicalDevice physical_device) {
  return PropertyChecker{GetPropertyNamesSet<VkExtensionProperties>(
      [physical_device](uint32_t* count, auto* properties) {
        return vkEnumerateDeviceExtensionProperties(
            physical_device, /*pLayerName=*/nullptr, count, properties);
      }
  )};
}

std::vector<std::string> PropertyChecker::FindUnsupported(
    absl::Span<const std::string> required_properties) const {
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
  LOG_EMPTY_LINE;

  return unsupported_properties;
}

}  // namespace lighter::renderer::vk
