add_subdirectory(ir)
add_subdirectory(vk)

if(LIGHTER_BUILD_LEGACY_APPS)
  add_subdirectory(vulkan)
endif()

add_library(lighter_renderer_util STATIC
  util.cc
  util.h
)
target_link_libraries(lighter_renderer_util
  PUBLIC
    lighter_project_options
    lighter_common_file
    lighter_common_util
    lighter_common_window
    lighter_renderer_ir_renderer
    lighter_renderer_ir_type
    lighter_renderer_vk_renderer
    absl::flags
)
