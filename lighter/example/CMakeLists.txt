function(lighter_add_example target)
  add_executable(${target} ${ARGN})
  add_dependencies(${target} lighter_shaders)
  target_link_libraries(${target}
    PRIVATE
      lighter_project_options
      lighter_base_data
      lighter_base_file
      lighter_base_flag
      lighter_base_timer
      lighter_base_util
      lighter_base_window
      lighter_renderer_vk_renderer
      lighter_scene_camera
      lighter_scene_util
      glm::glm
      lighter_spirv_cross
  )
endfunction()

lighter_add_example(example_triangle triangle.cc)

lighter_add_example(example_cube cube.cc)
target_link_libraries(example_cube
  PRIVATE
    lighter_scene_model
    lighter_ui_text
)

lighter_add_example(example_nanosuit nanosuit.cc)
target_link_libraries(example_nanosuit
  PRIVATE
    lighter_scene_model
    lighter_scene_skybox
)

lighter_add_example(example_planet planet.cc)
target_link_libraries(example_planet
  PRIVATE
    lighter_scene_model
    lighter_scene_skybox
)

lighter_add_example(example_post_effect post_effect.cc)
target_link_libraries(example_post_effect
  PRIVATE
    lighter_base_image
)

lighter_add_example(example_troop troop.cc)
target_link_libraries(example_troop
  PRIVATE
    lighter_scene_model
)

add_subdirectory(aurora)
