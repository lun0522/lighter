add_library(lighter_app_vulkan_common STATIC
  util.cc
  util.h
)
target_link_libraries(lighter_app_vulkan_common
  PUBLIC
    lighter_project_options
    lighter_common_camera
    lighter_common_data
    lighter_common_file
    lighter_common_image
    lighter_common_timer
    lighter_common_util
    lighter_renderer_util
    lighter_renderer_vulkan_extension_onscreen
    absl::flags_parse
    absl::span
    glm::glm
    Vulkan::Vulkan
)

add_library(lighter_app_vulkan_image_viewer STATIC
  image_viewer.cc
  image_viewer.h
)
target_link_libraries(lighter_app_vulkan_image_viewer
  PUBLIC
    lighter_app_vulkan_common
)

foreach(app_name IN ITEMS cube nanosuit planet triangle)
  add_executable(vulkan_${app_name} ${app_name}.cc)
  add_dependencies(vulkan_${app_name} lighter_shaders)
  target_link_libraries(vulkan_${app_name} PRIVATE lighter_app_vulkan_common)
endforeach()

add_executable(vulkan_post_effect post_effect.cc)
add_dependencies(vulkan_post_effect lighter_shaders)
target_link_libraries(vulkan_post_effect
  PRIVATE
    lighter_app_vulkan_common
    lighter_app_vulkan_image_viewer
)

add_subdirectory(troop)
add_subdirectory(aurora)
