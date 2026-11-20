add_library(lighter_renderer_vulkan_extension_offscreen_wrappers INTERFACE)
target_link_libraries(lighter_renderer_vulkan_extension_offscreen_wrappers
  INTERFACE
    lighter_renderer_vulkan_wrapper_basics
    lighter_renderer_vulkan_wrapper_buffer
    lighter_renderer_vulkan_wrapper_command
    lighter_renderer_vulkan_wrapper_descriptor
    lighter_renderer_vulkan_wrapper_image
    lighter_renderer_vulkan_wrapper_pipeline
    lighter_renderer_vulkan_wrapper_render_pass
    lighter_renderer_vulkan_wrapper_synchronization
    lighter_renderer_vulkan_wrapper_util
)

add_library(lighter_renderer_vulkan_extension_base_pass STATIC
  base_pass.cc
  base_pass.h
)
target_link_libraries(lighter_renderer_vulkan_extension_base_pass
  PUBLIC
    lighter_project_options
    lighter_common_util
    lighter_renderer_ir_image_usage
    lighter_renderer_vulkan_wrapper_image
    Vulkan::Vulkan
)

add_library(lighter_renderer_vulkan_extension_compute_pass STATIC
  compute_pass.cc
  compute_pass.h
)
target_link_libraries(lighter_renderer_vulkan_extension_compute_pass
  PUBLIC
    lighter_project_options
    lighter_common_util
    lighter_renderer_ir_image_usage
    lighter_renderer_vulkan_extension_base_pass
    lighter_renderer_vulkan_wrapper_image
    Vulkan::Vulkan
)

add_library(lighter_renderer_vulkan_extension_graphics_pass STATIC
  graphics_pass.cc
  graphics_pass.h
)
target_link_libraries(lighter_renderer_vulkan_extension_graphics_pass
  PUBLIC
    lighter_project_options
    lighter_common_util
    lighter_renderer_ir_image_usage
    lighter_renderer_vulkan_extension_base_pass
    lighter_renderer_vulkan_wrapper_image
    lighter_renderer_vulkan_wrapper_render_pass
)

add_library(lighter_renderer_vulkan_extension_model STATIC
  model.cc
  model.h
)
target_link_libraries(lighter_renderer_vulkan_extension_model
  PUBLIC
    lighter_project_options
    lighter_common_file
    lighter_common_model_loader
    lighter_common_util
    lighter_renderer_vulkan_extension_offscreen_wrappers
    Vulkan::Vulkan
)

add_library(lighter_renderer_vulkan_extension_naive_render_pass STATIC
  naive_render_pass.cc
  naive_render_pass.h
)
target_link_libraries(lighter_renderer_vulkan_extension_naive_render_pass
  PUBLIC
    lighter_project_options
    lighter_common_util
    lighter_renderer_vulkan_extension_graphics_pass
    lighter_renderer_vulkan_wrapper_basics
    lighter_renderer_vulkan_wrapper_render_pass
)

add_library(lighter_renderer_vulkan_extension_text STATIC
  text.cc
  text.h
  text_util.cc
  text_util.h
)
target_link_libraries(lighter_renderer_vulkan_extension_text
  PUBLIC
    lighter_project_options
    lighter_common_char_lib
    lighter_common_file
    lighter_common_image
    lighter_common_util
    lighter_renderer_util
    lighter_renderer_vulkan_extension_naive_render_pass
    lighter_renderer_vulkan_extension_offscreen_wrappers
    glm::glm
)

add_library(lighter_renderer_vulkan_extension_onscreen INTERFACE)
target_link_libraries(lighter_renderer_vulkan_extension_onscreen
  INTERFACE
    lighter_renderer_vulkan_extension_compute_pass
    lighter_renderer_vulkan_extension_graphics_pass
    lighter_renderer_vulkan_extension_model
    lighter_renderer_vulkan_extension_naive_render_pass
    lighter_renderer_vulkan_extension_offscreen_wrappers
    lighter_renderer_vulkan_extension_text
    lighter_renderer_vulkan_wrapper_window
)
