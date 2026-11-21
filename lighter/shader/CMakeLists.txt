add_subdirectory(compiler)

file(GLOB_RECURSE LIGHTER_SHADER_SOURCES CONFIGURE_DEPENDS
  "${LIGHTER_SHADER_DIR}/*.vert"
  "${LIGHTER_SHADER_DIR}/*.frag"
  "${LIGHTER_SHADER_DIR}/*.comp"
)

add_custom_target(lighter_shaders ALL
  COMMAND compile_shaders
    --shader_dir=${LIGHTER_SHADER_DIR}
    --opt_level=$<IF:$<CONFIG:Release>,perf,none>
  DEPENDS compile_shaders ${LIGHTER_SHADER_SOURCES}
  COMMENT "Compiling GLSL shaders to SPIR-V"
  VERBATIM
)
