//
//  util.h
//
//  Created by Pujun Lun on 5/14/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_SHADER_UTIL_H
#define LIGHTER_SHADER_UTIL_H

#include <string>
#include <string_view>

#include "lighter/common/graphics_api.h"

namespace lighter::shader::util {

std::string GetApiNameAbbreviation(common::GraphicsApi graphics_api);

}  // namespace lighter::shader::util

#endif  // LIGHTER_SHADER_UTIL_H
