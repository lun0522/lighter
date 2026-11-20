add_library(lighter_common_util STATIC
  util.cc
  util.h
)
target_link_libraries(lighter_common_util
  PUBLIC
    lighter_project_options
    absl::core_headers
    absl::flat_hash_set
    absl::function_ref
    absl::optimization
    absl::span
    absl::str_format
    absl::strings
)

add_library(lighter_common_data STATIC
  data.cc
  data.h
)
target_link_libraries(lighter_common_data
  PUBLIC
    lighter_project_options
    lighter_common_util
    absl::str_format
    glm::glm
)

add_library(lighter_common_file STATIC
  file.cc
  file.h
)
target_link_libraries(lighter_common_file
  PUBLIC
    lighter_project_options
    lighter_common_data
    lighter_common_util
    lighter_shader_compiler_util
    absl::flat_hash_map
    absl::flags
    absl::str_format
    absl::strings
    glm::glm
)
if(LIGHTER_RESOURCE_DIR)
  target_compile_definitions(lighter_common_file PRIVATE
    LIGHTER_RESOURCE_DIR="${LIGHTER_RESOURCE_DIR}"
  )
endif()
target_compile_definitions(lighter_common_file PRIVATE
  LIGHTER_SHADER_DIR="${LIGHTER_SHADER_DIR}"
)

add_library(lighter_common_image STATIC
  image.cc
  image.h
)
target_link_libraries(lighter_common_image
  PUBLIC
    lighter_project_options
    lighter_common_data
    lighter_common_file
    lighter_common_util
    absl::span
    absl::strings
    glm::glm
    stb::stb
)

add_library(lighter_common_char_lib STATIC
  char_lib.cc
  char_lib.h
)
target_link_libraries(lighter_common_char_lib
  PUBLIC
    lighter_project_options
    lighter_common_file
    lighter_common_image
    lighter_common_util
    absl::flat_hash_map
    absl::span
    freetype
    glm::glm
)

add_library(lighter_common_model_loader STATIC
  model_loader.cc
  model_loader.h
)
target_link_libraries(lighter_common_model_loader
  PUBLIC
    lighter_project_options
    lighter_common_file
    lighter_common_util
    absl::str_format
    assimp::assimp
)

add_library(lighter_common_camera STATIC
  camera.cc
  camera.h
)
target_link_libraries(lighter_common_camera
  PUBLIC
    lighter_project_options
    lighter_common_util
    absl::function_ref
    absl::memory
    glm::glm
)

add_library(lighter_common_timer INTERFACE
  timer.h
)
target_link_libraries(lighter_common_timer
  INTERFACE
    lighter_project_options
)

add_library(lighter_common_ref_count INTERFACE
  ref_count.h
)
target_link_libraries(lighter_common_ref_count
  INTERFACE
    lighter_project_options
    lighter_common_util
    absl::flat_hash_map
)

add_library(lighter_common_rotation STATIC
  rotation.cc
  rotation.h
)
target_link_libraries(lighter_common_rotation
  PUBLIC
    lighter_project_options
    lighter_common_camera
    lighter_common_timer
    lighter_common_util
    glm::glm
)

add_library(lighter_common_spline STATIC
  spline.cc
  spline.h
)
target_link_libraries(lighter_common_spline
  PUBLIC
    lighter_project_options
    lighter_common_util
    absl::span
    absl::str_format
    glm::glm
)

add_library(lighter_common_window STATIC
  window.cc
  window.h
)
target_link_libraries(lighter_common_window
  PUBLIC
    lighter_project_options
    lighter_common_util
    absl::flat_hash_map
    absl::str_format
    glfw
    glm::glm
    Vulkan::Vulkan
)
