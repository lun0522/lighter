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
    lighter_common_data
    lighter_common_file
    lighter_renderer_ir_type
    absl::span
)

add_library(lighter_renderer_ir_buffer_usage INTERFACE
  buffer_usage.h
)
target_link_libraries(lighter_renderer_ir_buffer_usage
  INTERFACE
    lighter_project_options
    lighter_common_util
    lighter_renderer_ir_type
)

add_library(lighter_renderer_ir_image_usage INTERFACE
  image_usage.h
)
target_link_libraries(lighter_renderer_ir_image_usage
  INTERFACE
    lighter_project_options
    lighter_common_util
    lighter_renderer_ir_type
    absl::flat_hash_map
    absl::span
    absl::str_format
)

add_library(lighter_renderer_ir_image INTERFACE
  image.h
)
target_link_libraries(lighter_renderer_ir_image
  INTERFACE
    lighter_project_options
    lighter_common_image
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
    lighter_common_util
    lighter_renderer_ir_buffer
    lighter_renderer_ir_type
    absl::flat_hash_map
    glm::glm
)

add_library(lighter_renderer_ir_pass INTERFACE
  pass.h
  pass_util.h
)
target_link_libraries(lighter_renderer_ir_pass
  INTERFACE
    lighter_project_options
    lighter_common_util
    lighter_renderer_ir_image
    lighter_renderer_ir_pipeline
    lighter_renderer_ir_type
    absl::flat_hash_map
)

add_library(lighter_renderer_ir_renderer INTERFACE
  renderer.h
)
target_link_libraries(lighter_renderer_ir_renderer
  INTERFACE
    lighter_project_options
    lighter_common_file
    lighter_common_image
    lighter_common_util
    lighter_common_window
    lighter_renderer_ir_buffer
    lighter_renderer_ir_buffer_usage
    lighter_renderer_ir_image
    lighter_renderer_ir_image_usage
    lighter_renderer_ir_pass
    lighter_renderer_ir_pipeline
    lighter_renderer_ir_type
    absl::span
    absl::str_format
    glm::glm
)
