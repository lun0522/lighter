add_library(lighter_shader_compiler_util STATIC
  util.cc
  util.h
)
target_link_libraries(lighter_shader_compiler_util
  PUBLIC
    lighter_project_options
)

add_library(lighter_shader_compiler_compilation_record STATIC
  compilation_record.cc
  compilation_record.h
)
target_link_libraries(lighter_shader_compiler_compilation_record
  PUBLIC
    lighter_project_options
    lighter_common_util
    lighter_shader_compiler_util
    absl::flat_hash_map
    absl::str_format
    absl::strings
)

add_library(lighter_shader_compiler_compiler STATIC
  compiler.cc
  compiler.h
)
target_link_libraries(lighter_shader_compiler_compiler
  PUBLIC
    lighter_project_options
    lighter_common_util
    lighter_shader_compiler_util
    absl::flat_hash_map
    absl::span
    Vulkan::shaderc_combined
)

add_library(lighter_shader_compiler_run STATIC
  run_compiler.cc
  run_compiler.h
)
target_link_libraries(lighter_shader_compiler_run
  PUBLIC
    lighter_project_options
    lighter_common_data
    lighter_common_file
    lighter_common_timer
    lighter_common_util
    lighter_shader_compiler_compilation_record
    lighter_shader_compiler_compiler
    lighter_shader_compiler_util
    absl::span
    absl::str_format
    picosha2::picosha2
)

add_executable(compile_shaders
  compile_shaders.cc
)
target_link_libraries(compile_shaders
  PRIVATE
    lighter_project_options
    lighter_common_util
    lighter_shader_compiler_run
    lighter_shader_compiler_util
    absl::flags
    absl::flags_parse
)
