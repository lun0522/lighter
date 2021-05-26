//
//  graphics_api.h
//
//  Created by Pujun Lun on 5/16/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_COMMON_GRAPHICS_API_H
#define LIGHTER_COMMON_GRAPHICS_API_H

#include <array>

namespace lighter::common::api {

constexpr const char kSpirvBinaryFileExtension[] = ".spv";

constexpr const char kShaderEntryPoint[] = "main";

enum class GraphicsApi { kOpengl, kVulkan };

constexpr int kNumSupportedApis = 2;

// Returns the full list of supported graphics APIs.
inline std::array<GraphicsApi, kNumSupportedApis> GetAllApis() {
  return {GraphicsApi::kOpengl, GraphicsApi::kVulkan};
}

// Returns the full name of 'api'.
const char* GetApiFullName(GraphicsApi api);

// Returns the abbreviated name of 'api'.
const char* GetApiAbbreviatedName(GraphicsApi api);

}  // namespace lighter::common::api

#endif  // LIGHTER_COMMON_GRAPHICS_API_H
