add_library(lighter_renderer_ir_type INTERFACE
  type.h
)
target_link_libraries(lighter_renderer_ir_type
  INTERFACE
    lighter_project_options
)

add_library(lighter_renderer_ir_buffer STATIC
  buffer_util.cc
  buffer.h
  buffer_util.h
)
target_link_libraries(lighter_renderer_ir_buffer
  PUBLIC
    lighter_project_options
    lighter_base_data
    lighter_base_file
    lighter_renderer_ir_type
)

add_library(lighter_renderer_ir_buffer_usage INTERFACE
  buffer_usage.h
)
target_link_libraries(lighter_renderer_ir_buffer_usage
  INTERFACE
    lighter_project_options
    lighter_base_util
    lighter_renderer_ir_type
)

add_library(lighter_renderer_ir_image_usage INTERFACE
  image_usage.h
)
target_link_libraries(lighter_renderer_ir_image_usage
  INTERFACE
    lighter_project_options
    lighter_base_util
    lighter_renderer_ir_type
)

add_library(lighter_renderer_ir_image INTERFACE
  image.h
)
target_link_libraries(lighter_renderer_ir_image
  INTERFACE
    lighter_project_options
    lighter_base_image
    lighter_renderer_ir_image_usage
    lighter_renderer_ir_type
    glm::glm
)

add_library(lighter_renderer_ir_pipeline STATIC
  pipeline_util.cc
  pipeline.h
  pipeline_util.h
)
target_link_libraries(lighter_renderer_ir_pipeline
  PUBLIC
    lighter_project_options
    lighter_base_util
    lighter_renderer_ir_buffer
    lighter_renderer_ir_type
    glm::glm
)

add_library(lighter_renderer_ir_render_graph STATIC
  render_graph.cc
  render_graph.h
  render_graph_util.cc
  render_graph_util.h
)
target_link_libraries(lighter_renderer_ir_render_graph
  PUBLIC
    lighter_project_options
    lighter_base_util
    lighter_renderer_ir_buffer
    lighter_renderer_ir_buffer_usage
    lighter_renderer_ir_image
    lighter_renderer_ir_image_usage
    lighter_renderer_ir_pipeline
    lighter_renderer_ir_type
    glm::glm
)

add_library(lighter_renderer_ir_renderer INTERFACE
  renderer.h
)
target_link_libraries(lighter_renderer_ir_renderer
  INTERFACE
    lighter_project_options
    lighter_base_file
    lighter_base_image
    lighter_base_util
    lighter_base_window
    lighter_renderer_ir_buffer
    lighter_renderer_ir_buffer_usage
    lighter_renderer_ir_image
    lighter_renderer_ir_image_usage
    lighter_renderer_ir_pipeline
    lighter_renderer_ir_render_graph
    lighter_renderer_ir_type
    glm::glm
)

add_executable(ir_render_graph_test
  render_graph_test.cc
)
target_link_libraries(ir_render_graph_test
  PRIVATE
    lighter_project_options
    lighter_renderer_ir_render_graph
    GTest::gtest_main
)
add_test(NAME ir_render_graph_test COMMAND ir_render_graph_test)

add_executable(ir_render_graph_util_test
  render_graph_util_test.cc
)
target_link_libraries(ir_render_graph_util_test
  PRIVATE
    lighter_project_options
    lighter_renderer_ir_render_graph
    GTest::gtest_main
)
add_test(NAME ir_render_graph_util_test COMMAND ir_render_graph_util_test)
