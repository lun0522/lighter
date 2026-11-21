cmake_minimum_required(VERSION 3.24)

# Allow older third-party subprojects to configure under CMake 4.x.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

project(lighter LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LIGHTER_FETCH_RESOURCE
       "Fetch the lighter resource repository if LIGHTER_RESOURCE_DIR is unset"
       ON)

set(LIGHTER_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lighter/shader"
    CACHE PATH "Directory containing GLSL sources and compiled SPIR-V binaries")

if(NOT LIGHTER_RESOURCE_DIR)
  set(_lighter_sibling_resource "${CMAKE_CURRENT_SOURCE_DIR}/../resource")
  if(EXISTS "${_lighter_sibling_resource}")
    get_filename_component(_lighter_sibling_resource_abs
                           "${_lighter_sibling_resource}" ABSOLUTE)
    set(LIGHTER_RESOURCE_DIR "${_lighter_sibling_resource_abs}"
        CACHE PATH "Directory containing runtime assets (models, textures, fonts)")
  endif()
endif()

include(cmake/Dependencies.cmake)

enable_testing()

add_library(lighter_project_options INTERFACE)
target_include_directories(lighter_project_options INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_definitions(lighter_project_options INTERFACE
  GLM_ENABLE_EXPERIMENTAL
  GLM_FORCE_DEPTH_ZERO_TO_ONE
)

add_subdirectory(lighter/common)
add_subdirectory(lighter/shader_compiler)
add_subdirectory(lighter/shader)
add_subdirectory(lighter/renderer)
add_subdirectory(lighter/example)
