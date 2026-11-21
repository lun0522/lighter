//
//  run_compiler.h
//
//  Created by Pujun Lun on 5/17/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_SHADER_COMPILER_RUN_COMPILER_H
#define LIGHTER_SHADER_COMPILER_RUN_COMPILER_H

#include <filesystem>

#include "lighter/shader/compiler/compiler.h"

namespace lighter::shader::compiler::compiler {

// Compiles all shader files in 'shader_dir', which must be a valid directory.
void CompileShaders(std::filesystem::path&& shader_dir,
                    OptimizationLevel opt_level);

}  // namespace lighter::shader::compiler::compiler

#endif  // LIGHTER_SHADER_COMPILER_RUN_COMPILER_H
