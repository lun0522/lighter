add_library(lighter_base_util STATIC
  util.cc
  util.h
)
target_link_libraries(lighter_base_util
  PUBLIC
    lighter_project_options
)

add_library(lighter_base_flag STATIC
  flag.cc
  flag.h
)
target_link_libraries(lighter_base_flag
  PUBLIC
    lighter_project_options
    lighter_base_util
)

add_library(lighter_base_data STATIC
  data.cc
  data.h
)
target_link_libraries(lighter_base_data
  PUBLIC
    lighter_project_options
    lighter_base_util
    glm::glm
)

add_library(lighter_base_file STATIC
  file.cc
  file.h
)
target_link_libraries(lighter_base_file
  PUBLIC
    lighter_project_options
    lighter_base_data
    lighter_base_flag
    lighter_base_util
    glm::glm
)
if(LIGHTER_RESOURCE_DIR)
  target_compile_definitions(lighter_base_file PRIVATE
    LIGHTER_RESOURCE_DIR="${LIGHTER_RESOURCE_DIR}"
  )
endif()
target_compile_definitions(lighter_base_file PRIVATE
  LIGHTER_SHADER_DIR="${LIGHTER_SHADER_DIR}"
)

add_library(lighter_base_image STATIC
  image.cc
  image.h
)
target_link_libraries(lighter_base_image
  PUBLIC
    lighter_project_options
    lighter_base_data
    lighter_base_file
    lighter_base_util
    glm::glm
    stb::stb
)

add_library(lighter_base_timer INTERFACE
  timer.h
)
target_link_libraries(lighter_base_timer
  INTERFACE
    lighter_project_options
)

add_library(lighter_base_ref_count INTERFACE
  ref_count.h
)
target_link_libraries(lighter_base_ref_count
  INTERFACE
    lighter_project_options
    lighter_base_util
)

add_library(lighter_base_window STATIC
  window.cc
  window.h
)
target_link_libraries(lighter_base_window
  PUBLIC
    lighter_project_options
    lighter_base_util
    glfw
    glm::glm
    Vulkan::Vulkan
)

add_executable(base_flag_test
  flag_test.cc
)
target_link_libraries(base_flag_test
  PRIVATE
    lighter_project_options
    lighter_base_flag
    GTest::gtest_main
)
add_test(NAME base_flag_test COMMAND base_flag_test)

add_executable(base_ref_count_test
  ref_count_test.cc
)
target_link_libraries(base_ref_count_test
  PRIVATE
    lighter_project_options
    lighter_base_ref_count
    GTest::gtest_main
)
add_test(NAME base_ref_count_test COMMAND base_ref_count_test)
