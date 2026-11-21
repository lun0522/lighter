function(lighter_add_example target)
  add_executable(${target} ${ARGN} util.h)
  add_dependencies(${target} lighter_shaders)
  target_link_libraries(${target}
    PRIVATE
      lighter_project_options
      lighter_common_camera
      lighter_common_data
      lighter_common_file
      lighter_common_flag
      lighter_common_timer
      lighter_common_util
      lighter_common_window
      lighter_renderer_util
      lighter_renderer_vk_renderer
      glm::glm
      lighter_spirv_cross
  )
endfunction()

lighter_add_example(example_triangle triangle.cc)

lighter_add_example(example_cube cube.cc)
target_link_libraries(example_cube
  PRIVATE
    lighter_renderer_extension_model
    lighter_renderer_extension_text
)

lighter_add_example(example_nanosuit nanosuit.cc)
target_link_libraries(example_nanosuit
  PRIVATE
    lighter_renderer_extension_model
)

lighter_add_example(example_planet planet.cc)
target_link_libraries(example_planet
  PRIVATE
    lighter_renderer_extension_model
)

lighter_add_example(example_post_effect post_effect.cc)
target_link_libraries(example_post_effect
  PRIVATE
    lighter_common_image
)

lighter_add_example(example_troop troop.cc)
target_link_libraries(example_troop
  PRIVATE
    lighter_renderer_extension_model
)
