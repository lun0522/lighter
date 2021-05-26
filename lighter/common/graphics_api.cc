//
//  graphics_api.cc
//
//  Created by Pujun Lun on 5/21/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/common/graphics_api.h"

namespace lighter::common::api {

const char* GetApiFullName(GraphicsApi api) {
  switch (api) {
    case GraphicsApi::kOpengl:
      return "OpenGL";
    case GraphicsApi::kVulkan:
      return "Vulkan";
  }
}

const char* GetApiAbbreviatedName(GraphicsApi api) {
  switch (api) {
    case GraphicsApi::kOpengl:
      return "gl";
    case GraphicsApi::kVulkan:
      return "vk";
  }
}

}  // namespace lighter::common::api
