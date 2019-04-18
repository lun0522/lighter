package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
    name = "assimp",
    srcs = ["libassimp.4.1.0.dylib"],
    deps = ["@lib-assimp//:assimp"],
)

cc_library(
    name = "freetype",
    srcs = ["libfreetype.6.dylib"],
    deps = ["@lib-freetype//:freetype"],
)

cc_library(
    name = "glfw",
    srcs = ["libglfw.3.3.dylib"],
    deps = ["@lib-glfw//:glfw"],
)

cc_library(
    name = "glm",
    deps = ["@lib-glm//:glm"],
)

cc_library(
    name = "stb",
    deps = ["@lib-stb//:stb"],
)

cc_library(
    name = "vulkan",
    deps = ["@lib-vulkan//:vulkan"],
)