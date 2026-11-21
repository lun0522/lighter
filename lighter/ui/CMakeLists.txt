add_library(lighter_ui_char_lib STATIC
  char_lib.cc
  char_lib.h
)
target_link_libraries(lighter_ui_char_lib
  PUBLIC
    lighter_project_options
    lighter_base_file
    lighter_base_image
    lighter_base_util
    freetype
    glm::glm
)

add_library(lighter_ui_text STATIC
  text.cc
  text.h
  text_util.cc
  text_util.h
)
target_link_libraries(lighter_ui_text
  PUBLIC
    lighter_project_options
    lighter_base_data
    lighter_base_file
    lighter_base_image
    lighter_base_util
    lighter_ui_char_lib
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

add_library(lighter_ui_button STATIC
  button.cc
  button.h
  button_maker.cc
  button_maker.h
)
target_link_libraries(lighter_ui_button
  PUBLIC
    lighter_project_options
    lighter_base_data
    lighter_base_file
    lighter_base_image
    lighter_base_util
    lighter_ui_text
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

add_executable(ui_text_util_test
  text_util_test.cc
)
target_link_libraries(ui_text_util_test
  PRIVATE
    lighter_project_options
    lighter_ui_text
    GTest::gtest_main
)
add_test(NAME ui_text_util_test COMMAND ui_text_util_test)
