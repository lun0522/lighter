add_executable(vulkan_troop
  geometry_pass.cc
  geometry_pass.h
  lighting_pass.cc
  lighting_pass.h
  troop.cc
)
add_dependencies(vulkan_troop lighter_shaders)
target_link_libraries(vulkan_troop
  PRIVATE
    lighter_app_vulkan_common
    glm::glm
)
