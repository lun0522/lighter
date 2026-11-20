include(FetchContent)

find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

if(NOT TARGET Vulkan::shaderc_combined)
  find_library(LIGHTER_SHADERC_COMBINED_LIB
    NAMES shaderc_combined
    HINTS "$ENV{VULKAN_SDK}/lib" "${Vulkan_INCLUDE_DIR}/../lib"
  )
  if(NOT LIGHTER_SHADERC_COMBINED_LIB)
    message(FATAL_ERROR
      "Could not find shaderc_combined from the Vulkan SDK.")
  endif()
  add_library(Vulkan::shaderc_combined UNKNOWN IMPORTED)
  set_target_properties(Vulkan::shaderc_combined PROPERTIES
    IMPORTED_LOCATION "${LIGHTER_SHADERC_COMBINED_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${Vulkan_INCLUDE_DIRS}"
  )
endif()

# Abseil (temporary until replaced by C++20 STL in Commit 6).
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  absl
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG 20240722.0
  GIT_SHALLOW TRUE
  FIND_PACKAGE_ARGS NAMES absl
)

# GLFW.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
  GIT_SHALLOW TRUE
  FIND_PACKAGE_ARGS NAMES glfw3
)

# GLM.
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
  GIT_SHALLOW TRUE
  FIND_PACKAGE_ARGS NAMES glm
)

# FreeType.
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  freetype
  GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
  GIT_TAG VER-2-13-3
  GIT_SHALLOW TRUE
  FIND_PACKAGE_ARGS NAMES Freetype
)

# Assimp (Wavefront OBJ importer only, matching original configuration).
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  assimp
  GIT_REPOSITORY https://github.com/assimp/assimp.git
  GIT_TAG v5.4.3
  GIT_SHALLOW TRUE
  FIND_PACKAGE_ARGS NAMES assimp
)

# Vulkan Memory Allocator (VMA).
FetchContent_Declare(
  VulkanMemoryAllocator
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG v3.1.0
  GIT_SHALLOW TRUE
  FIND_PACKAGE_ARGS NAMES VulkanMemoryAllocator
)

# SPIRV-Cross.
set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  spirv_cross
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
  GIT_TAG vulkan-sdk-1.3.296.0
  GIT_SHALLOW TRUE
)

# GoogleTest.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
  GIT_SHALLOW TRUE
  FIND_PACKAGE_ARGS NAMES GTest
)

# Header-only libraries without CMake build files.
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG c9064e317699d2e495f36ba4f9ac037e88ee371a
)

FetchContent_Declare(
  picosha2
  GIT_REPOSITORY https://github.com/okdshin/PicoSHA2.git
  GIT_TAG b699e6c900be6e00152db5a3d123c1db42ea13d0
)

FetchContent_MakeAvailable(
  absl
  glfw
  glm
  freetype
  assimp
  VulkanMemoryAllocator
  spirv_cross
  googletest
)

if(NOT TARGET stb::stb)
  FetchContent_GetProperties(stb)
  if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
  endif()
  add_library(stb_headers INTERFACE)
  target_include_directories(stb_headers INTERFACE "${stb_SOURCE_DIR}")
  add_library(stb::stb ALIAS stb_headers)
endif()

if(NOT TARGET picosha2::picosha2)
  FetchContent_GetProperties(picosha2)
  if(NOT picosha2_POPULATED)
    FetchContent_Populate(picosha2)
  endif()
  add_library(picosha2_headers INTERFACE)
  target_include_directories(picosha2_headers INTERFACE "${picosha2_SOURCE_DIR}")
  add_library(picosha2::picosha2 ALIAS picosha2_headers)
endif()

# Expose SPIRV-Cross headers under <spirv_cross/...> to match project includes.
if(TARGET spirv-cross-core AND spirv_cross_SOURCE_DIR)
  set(_spirv_cross_inc_root "${CMAKE_BINARY_DIR}/_generated_includes")
  file(MAKE_DIRECTORY "${_spirv_cross_inc_root}")
  if(NOT EXISTS "${_spirv_cross_inc_root}/spirv_cross")
    file(CREATE_LINK "${spirv_cross_SOURCE_DIR}"
         "${_spirv_cross_inc_root}/spirv_cross" SYMBOLIC)
  endif()
  target_include_directories(spirv-cross-core INTERFACE
    "${_spirv_cross_inc_root}"
  )
endif()

# Runtime resource assets fallback if LIGHTER_RESOURCE_DIR was not found locally.
if(NOT LIGHTER_RESOURCE_DIR AND LIGHTER_FETCH_RESOURCE)
  FetchContent_Declare(
    lighter_resource
    URL https://github.com/lun0522/resource/archive/refs/tags/1.0.0.tar.gz
  )
  FetchContent_GetProperties(lighter_resource)
  if(NOT lighter_resource_POPULATED)
    FetchContent_Populate(lighter_resource)
  endif()
  set(LIGHTER_RESOURCE_DIR "${lighter_resource_SOURCE_DIR}"
      CACHE PATH "Directory containing runtime assets (models, textures, fonts)"
      FORCE)
endif()
