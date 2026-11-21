add_library(lighter_renderer_vk_util INTERFACE
  util.h
)
target_compile_definitions(lighter_renderer_vk_util
  INTERFACE
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
)
target_link_libraries(lighter_renderer_vk_util
  INTERFACE
    lighter_project_options
    lighter_common_image
    lighter_common_util
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
)

add_library(lighter_renderer_vk_context STATIC
  basic.cc
  basic.h
  context.cc
  context.h
  vma_impl.cc
)
target_compile_definitions(lighter_renderer_vk_context
  PUBLIC
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
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
    GPUOpen::VulkanMemoryAllocator
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
    lighter_renderer_ir_pipeline
    lighter_renderer_vk_buffer
    lighter_renderer_vk_context
    lighter_renderer_vk_image
    lighter_renderer_vk_type_mapping
    lighter_renderer_vk_util
    lighter_shader_compiler_util
  PRIVATE
    lighter_spirv_cross
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
)

add_library(lighter_renderer_vk_command STATIC
  command.cc
  command.h
)
target_link_libraries(lighter_renderer_vk_command
  PUBLIC
    lighter_project_options
    lighter_common_util
    lighter_renderer_vk_context
    lighter_renderer_vk_swapchain
    lighter_renderer_vk_util
)

add_library(lighter_renderer_vk_transient_resource_pool STATIC
  transient_resource_pool.cc
  transient_resource_pool.h
)
target_link_libraries(lighter_renderer_vk_transient_resource_pool
  PUBLIC
    lighter_project_options
    lighter_common_util
    lighter_renderer_ir_render_graph
    lighter_renderer_ir_type
    lighter_renderer_vk_buffer
    lighter_renderer_vk_context
    lighter_renderer_vk_image
    lighter_renderer_vk_util
)

add_library(lighter_renderer_vk_pipeline_cache STATIC
  pipeline_cache.cc
  pipeline_cache.h
)
target_link_libraries(lighter_renderer_vk_pipeline_cache
  PUBLIC
    lighter_project_options
    lighter_renderer_ir_image
    lighter_renderer_ir_pipeline
    lighter_renderer_vk_context
    lighter_renderer_vk_image
    lighter_renderer_vk_pipeline
    lighter_renderer_vk_util
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
    lighter_renderer_vk_command
    lighter_renderer_vk_context
    lighter_renderer_vk_image
    lighter_renderer_vk_pipeline
    lighter_renderer_vk_swapchain
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

add_executable(vk_buffer_util_test
  buffer_util_test.cc
)
target_link_libraries(vk_buffer_util_test
  PRIVATE
    lighter_project_options
    lighter_renderer_ir_type
    lighter_renderer_vk_buffer
    GTest::gtest_main
)
add_test(NAME vk_buffer_util_test COMMAND vk_buffer_util_test)

add_executable(vk_pipeline_test
  pipeline_test.cc
)
target_link_libraries(vk_pipeline_test
  PRIVATE
    lighter_project_options
    lighter_common_data
    lighter_common_file
    lighter_renderer_vk_pipeline
    GTest::gtest_main
)
add_test(NAME vk_pipeline_test COMMAND vk_pipeline_test)

add_executable(vk_transient_resource_pool_test
  transient_resource_pool_test.cc
)
target_link_libraries(vk_transient_resource_pool_test
  PRIVATE
    lighter_project_options
    lighter_renderer_vk_transient_resource_pool
    GTest::gtest_main
)
add_test(NAME vk_transient_resource_pool_test
         COMMAND vk_transient_resource_pool_test)

add_executable(vk_pipeline_cache_test
  pipeline_cache_test.cc
)
target_link_libraries(vk_pipeline_cache_test
  PRIVATE
    lighter_project_options
    lighter_renderer_vk_pipeline_cache
    GTest::gtest_main
)
add_test(NAME vk_pipeline_cache_test COMMAND vk_pipeline_cache_test)
