add_library(lighter_renderer_vk_util INTERFACE
  util.h
)
target_link_libraries(lighter_renderer_vk_util
  INTERFACE
    lighter_project_options
    lighter_common_image
    lighter_common_util
    absl::str_format
    glm::glm
    Vulkan::Vulkan
)

add_library(lighter_renderer_vk_type_mapping STATIC
  type_mapping.cc
  type_mapping.h
)
target_link_libraries(lighter_renderer_vk_type_mapping
  PUBLIC
    lighter_project_options
    lighter_common_util
    lighter_renderer_ir_type
    lighter_renderer_vk_util
)

add_library(lighter_renderer_vk_property_checker STATIC
  property_checker.cc
  property_checker.h
)
target_link_libraries(lighter_renderer_vk_property_checker
  PUBLIC
    lighter_project_options
    lighter_common_util
    lighter_renderer_vk_util
    absl::flat_hash_set
    absl::span
)

add_library(lighter_renderer_vk_context STATIC
  basic.cc
  basic.h
  context.cc
  context.h
)
target_link_libraries(lighter_renderer_vk_context
  PUBLIC
    lighter_project_options
    lighter_common_util
    lighter_common_window
    lighter_renderer_ir_pipeline
    lighter_renderer_ir_type
    lighter_renderer_vk_property_checker
    lighter_renderer_vk_type_mapping
    lighter_renderer_vk_util
    absl::flat_hash_map
    absl::flat_hash_set
    absl::span
    absl::str_format
)

add_library(lighter_renderer_vk_buffer STATIC
  buffer.cc
  buffer.h
  buffer_util.cc
  buffer_util.h
)
target_link_libraries(lighter_renderer_vk_buffer
  PUBLIC
    lighter_project_options
    lighter_common_util
    lighter_renderer_ir_buffer
    lighter_renderer_ir_buffer_usage
    lighter_renderer_ir_type
    lighter_renderer_vk_context
    lighter_renderer_vk_util
    absl::flat_hash_set
    absl::span
)

add_library(lighter_renderer_vk_image STATIC
  image.cc
  image.h
  image_util.cc
  image_util.h
)
target_link_libraries(lighter_renderer_vk_image
  PUBLIC
    lighter_project_options
    lighter_common_image
    lighter_common_util
    lighter_renderer_ir_image
    lighter_renderer_ir_image_usage
    lighter_renderer_ir_type
    lighter_renderer_vk_buffer
    lighter_renderer_vk_context
    lighter_renderer_vk_type_mapping
    lighter_renderer_vk_util
    absl::flat_hash_set
    absl::memory
    absl::span
    absl::str_format
    glm::glm
)

add_library(lighter_renderer_vk_pipeline STATIC
  pipeline.cc
  pipeline.h
)
target_link_libraries(lighter_renderer_vk_pipeline
  PUBLIC
    lighter_project_options
    lighter_common_data
    lighter_common_file
    lighter_common_ref_count
    lighter_common_util
    lighter_renderer_ir_image
    lighter_renderer_ir_image_usage
    lighter_renderer_ir_pass
    lighter_renderer_ir_pipeline
    lighter_renderer_vk_context
    lighter_renderer_vk_type_mapping
    lighter_renderer_vk_util
    lighter_shader_compiler_util
    absl::span
    absl::str_format
)

add_library(lighter_renderer_vk_render_pass STATIC
  framebuffer.cc
  framebuffer.h
  render_pass.cc
  render_pass.h
)
target_link_libraries(lighter_renderer_vk_render_pass
  PUBLIC
    lighter_project_options
    lighter_renderer_ir_image
    lighter_renderer_ir_pass
    lighter_renderer_vk_context
    lighter_renderer_vk_image
    lighter_renderer_vk_type_mapping
    lighter_renderer_vk_util
    absl::flat_hash_map
    absl::span
    absl::str_format
)

add_library(lighter_renderer_vk_swapchain STATIC
  swapchain.cc
  swapchain.h
)
target_link_libraries(lighter_renderer_vk_swapchain
  PUBLIC
    lighter_project_options
    lighter_common_image
    lighter_common_util
    lighter_renderer_ir_image_usage
    lighter_renderer_ir_type
    lighter_renderer_vk_context
    lighter_renderer_vk_image
    lighter_renderer_vk_util
    absl::flat_hash_set
    absl::span
    absl::str_format
)

add_library(lighter_renderer_vk_renderer STATIC
  renderer.cc
  renderer.h
)
target_link_libraries(lighter_renderer_vk_renderer
  PUBLIC
    lighter_project_options
    lighter_common_image
    lighter_common_window
    lighter_renderer_ir_buffer_usage
    lighter_renderer_ir_image_usage
    lighter_renderer_ir_renderer
    lighter_renderer_ir_type
    lighter_renderer_vk_buffer
    lighter_renderer_vk_context
    lighter_renderer_vk_image
    lighter_renderer_vk_pipeline
    lighter_renderer_vk_render_pass
    lighter_renderer_vk_swapchain
    absl::span
    glm::glm
)

add_executable(vk_image_util_test
  image_util_test.cc
)
target_link_libraries(vk_image_util_test
  PRIVATE
    lighter_project_options
    lighter_renderer_ir_type
    lighter_renderer_vk_image
    GTest::gtest_main
)
add_test(NAME vk_image_util_test COMMAND vk_image_util_test)
