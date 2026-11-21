add_library(lighter_scene_model_loader STATIC
  model_loader.cc
  model_loader.h
)
target_link_libraries(lighter_scene_model_loader
  PUBLIC
    lighter_project_options
    lighter_base_file
    lighter_base_util
    assimp::assimp
)

add_library(lighter_scene_model STATIC
  model.cc
  model.h
)
target_link_libraries(lighter_scene_model
  PUBLIC
    lighter_project_options
    lighter_base_file
    lighter_base_image
    lighter_base_util
    lighter_scene_model_loader
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

add_library(lighter_scene_skybox STATIC
  skybox.cc
  skybox.h
)
target_link_libraries(lighter_scene_skybox
  PUBLIC
    lighter_project_options
    lighter_base_data
    lighter_base_file
    lighter_scene_model
    lighter_renderer_ir_pipeline
    lighter_renderer_ir_render_graph
    lighter_renderer_ir_renderer
    lighter_renderer_ir_type
    glm::glm
)

add_library(lighter_scene_camera STATIC
  camera.cc
  camera.h
)
target_link_libraries(lighter_scene_camera
  PUBLIC
    lighter_project_options
    lighter_base_util
    glm::glm
)

add_library(lighter_scene_rotation STATIC
  rotation.cc
  rotation.h
)
target_link_libraries(lighter_scene_rotation
  PUBLIC
    lighter_project_options
    lighter_base_timer
    lighter_base_util
    lighter_scene_camera
    glm::glm
)

add_library(lighter_scene_spline STATIC
  spline.cc
  spline.h
)
target_link_libraries(lighter_scene_spline
  PUBLIC
    lighter_project_options
    lighter_base_util
    glm::glm
)

add_library(lighter_scene_distance_field STATIC
  distance_field.cc
  distance_field.h
)
target_link_libraries(lighter_scene_distance_field
  PUBLIC
    lighter_project_options
    lighter_base_data
    lighter_base_file
    lighter_renderer_ir_image_usage
    lighter_renderer_ir_pipeline
    lighter_renderer_ir_render_graph
    lighter_renderer_ir_type
    glm::glm
)

add_library(lighter_scene_scene INTERFACE
  scene.h
)
target_link_libraries(lighter_scene_scene
  INTERFACE
    lighter_project_options
    lighter_renderer_ir_render_graph
)

add_library(lighter_scene_util INTERFACE
  util.h
)
target_link_libraries(lighter_scene_util
  INTERFACE
    lighter_project_options
    lighter_base_data
    lighter_base_file
    lighter_base_flag
    lighter_base_timer
    lighter_base_util
    lighter_base_window
    lighter_renderer_ir_buffer
    lighter_renderer_ir_image
    lighter_renderer_ir_pipeline
    lighter_renderer_ir_render_graph
    lighter_renderer_ir_renderer
    lighter_renderer_ir_type
    lighter_renderer_vk_renderer
    lighter_scene_camera
    glm::glm
)

add_executable(scene_spline_test
  spline_test.cc
)
target_link_libraries(scene_spline_test
  PRIVATE
    lighter_project_options
    lighter_scene_spline
    GTest::gtest_main
)
add_test(NAME scene_spline_test COMMAND scene_spline_test)
