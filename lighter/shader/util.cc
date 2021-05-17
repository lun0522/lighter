//
//  util.cc
//
//  Created by Pujun Lun on 5/14/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/shader/util.h"

namespace lighter::shader::util {
namespace {

using common::GraphicsApi;

}  // namespace

std::string GetApiNameAbbreviation(GraphicsApi graphics_api) {
  switch (graphics_api) {
    case GraphicsApi::kOpengl:
      return "gl";
    case GraphicsApi::kVulkan:
      return "vk";
  }
}

}  // namespace lighter::shader::util
