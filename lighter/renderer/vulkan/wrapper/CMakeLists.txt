add_library(lighter_renderer_vulkan_wrapper_util STATIC
  util.cc
  util.h
)
target_link_libraries(lighter_renderer_vulkan_wrapper_util
  PUBLIC
    lighter_project_options
    lighter_common_util
    absl::str_format
    glm::glm
    Vulkan::Vulkan
)

add_library(lighter_renderer_vulkan_wrapper_basics STATIC
  basic_context.h
  basic_object.cc
  basic_object.h
  $<$<NOT:$<CONFIG:Release>>:validation.cc>
  $<$<NOT:$<CONFIG:Release>>:validation.h>
)
target_link_libraries(lighter_renderer_vulkan_wrapper_basics
  PUBLIC
    lighter_project_options
    lighter_common_ref_count
    lighter_common_util
    lighter_renderer_vulkan_wrapper_util
    absl::flat_hash_map
    absl::flat_hash_set
    absl::memory
    absl::span
    absl::str_format
    absl::strings
    Vulkan::Vulkan
)

add_library(lighter_renderer_vulkan_wrapper_synchronization STATIC
  synchronization.cc
  synchronization.h
)
target_link_libraries(lighter_renderer_vulkan_wrapper_synchronization
  PUBLIC
    lighter_project_options
    lighter_renderer_vulkan_wrapper_basics
    lighter_renderer_vulkan_wrapper_util
    Vulkan::Vulkan
)

add_library(lighter_renderer_vulkan_wrapper_command STATIC
  command.cc
  command.h
)
target_link_libraries(lighter_renderer_vulkan_wrapper_command
  PUBLIC
    lighter_project_options
    lighter_common_util
    lighter_renderer_vulkan_wrapper_basics
    lighter_renderer_vulkan_wrapper_synchronization
    lighter_renderer_vulkan_wrapper_util
    absl::str_format
    Vulkan::Vulkan
)

add_library(lighter_renderer_vulkan_wrapper_buffer STATIC
  buffer.cc
  buffer.h
)
target_link_libraries(lighter_renderer_vulkan_wrapper_buffer
  PUBLIC
    lighter_project_options
    lighter_common_util
    lighter_renderer_vulkan_wrapper_basics
    lighter_renderer_vulkan_wrapper_command
    lighter_renderer_vulkan_wrapper_util
    absl::span
    absl::str_format
    Vulkan::Vulkan
)

add_library(lighter_renderer_vulkan_wrapper_descriptor STATIC
  descriptor.cc
  descriptor.h
)
target_link_libraries(lighter_renderer_vulkan_wrapper_descriptor
  PUBLIC
    lighter_project_options
    lighter_common_model_loader
    lighter_common_util
    lighter_renderer_vulkan_wrapper_basics
    lighter_renderer_vulkan_wrapper_util
    absl::flat_hash_map
    absl::span
    absl::str_format
    Vulkan::Vulkan
)

add_library(lighter_renderer_vulkan_wrapper_image STATIC
  image.cc
  image.h
  image_util.cc
  image_util.h
)
target_link_libraries(lighter_renderer_vulkan_wrapper_image
  PUBLIC
    lighter_project_options
    lighter_common_file
    lighter_common_image
    lighter_common_ref_count
    lighter_common_util
    lighter_renderer_ir_image_usage
    lighter_renderer_vulkan_wrapper_basics
    lighter_renderer_vulkan_wrapper_buffer
    lighter_renderer_vulkan_wrapper_command
    lighter_renderer_vulkan_wrapper_util
    absl::flat_hash_set
    absl::memory
    absl::span
    absl::str_format
    Vulkan::Vulkan
)

add_library(lighter_renderer_vulkan_wrapper_pipeline STATIC
  pipeline.cc
  pipeline.h
  pipeline_util.cc
  pipeline_util.h
)
target_link_libraries(lighter_renderer_vulkan_wrapper_pipeline
  PUBLIC
    lighter_project_options
    lighter_common_data
    lighter_common_file
    lighter_common_ref_count
    lighter_common_util
    lighter_renderer_vulkan_wrapper_basics
    lighter_renderer_vulkan_wrapper_buffer
    lighter_renderer_vulkan_wrapper_util
    absl::flat_hash_map
    absl::span
    absl::str_format
    Vulkan::Vulkan
)

add_library(lighter_renderer_vulkan_wrapper_render_pass STATIC
  render_pass.cc
  render_pass.h
)
target_link_libraries(lighter_renderer_vulkan_wrapper_render_pass
  PUBLIC
    lighter_project_options
    lighter_common_image
    lighter_common_util
    lighter_renderer_vulkan_wrapper_basics
    lighter_renderer_vulkan_wrapper_image
    lighter_renderer_vulkan_wrapper_util
    absl::span
    absl::str_format
    Vulkan::Vulkan
)

add_library(lighter_renderer_vulkan_wrapper_window STATIC
  swapchain.cc
  swapchain.h
  window_context.h
)
target_link_libraries(lighter_renderer_vulkan_wrapper_window
  PUBLIC
    lighter_project_options
    lighter_common_image
    lighter_common_util
    lighter_common_window
    lighter_renderer_vulkan_wrapper_basics
    lighter_renderer_vulkan_wrapper_image
    lighter_renderer_vulkan_wrapper_util
    Vulkan::Vulkan
)

add_executable(vulkan_image_util_test
  image_util_test.cc
)
target_link_libraries(vulkan_image_util_test
  PRIVATE
    lighter_project_options
    lighter_renderer_vulkan_wrapper_image
    GTest::gtest_main
    Vulkan::Vulkan
)
add_test(NAME vulkan_image_util_test COMMAND vulkan_image_util_test)
