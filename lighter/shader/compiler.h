//
//  compiler.h
//
//  Created by Pujun Lun on 5/17/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_SHADER_COMPILER_H
#define LIGHTER_SHADER_COMPILER_H

#include <filesystem>

namespace lighter::shader::compiler {

// Compiles all shader files in 'shader_dir', which must be a valid directory.
void Compile(const std::filesystem::path& shader_dir);

}  // namespace lighter::shader::compiler

#endif  // LIGHTER_SHADER_COMPILER_H
