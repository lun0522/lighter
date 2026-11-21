add_library(lighter_renderer_extension_model STATIC
  model.cc
  model.h
)
target_link_libraries(lighter_renderer_extension_model
  PUBLIC
    lighter_project_options
    lighter_common_file
    lighter_common_image
    lighter_common_model_loader
    lighter_common_util
    lighter_renderer_ir_buffer
    lighter_renderer_ir_buffer_usage
    lighter_renderer_ir_image
    lighter_renderer_ir_image_usage
    lighter_renderer_ir_pipeline
    lighter_renderer_ir_render_graph
    lighter_renderer_ir_renderer
    lighter_renderer_ir_type
    glm::glm
)

add_library(lighter_renderer_extension_text STATIC
  text.cc
  text.h
  text_util.cc
  text_util.h
)
target_link_libraries(lighter_renderer_extension_text
  PUBLIC
    lighter_project_options
    lighter_common_char_lib
    lighter_common_data
    lighter_common_file
    lighter_common_image
    lighter_common_util
    lighter_renderer_ir_buffer
    lighter_renderer_ir_buffer_usage
    lighter_renderer_ir_image
    lighter_renderer_ir_image_usage
    lighter_renderer_ir_pipeline
    lighter_renderer_ir_render_graph
    lighter_renderer_ir_renderer
    lighter_renderer_ir_type
    glm::glm
)

add_executable(extension_test
  extension_test.cc
)
target_link_libraries(extension_test
  PRIVATE
    lighter_project_options
    lighter_renderer_extension_model
    lighter_renderer_extension_text
    GTest::gtest_main
)
add_test(NAME extension_test COMMAND extension_test)
