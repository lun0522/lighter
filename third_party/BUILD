package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
    name = "freetype",
    srcs = ["libfreetype.6.dylib"],
    deps = ["@lib-freetype//:freetype"],
)

cc_library(
    name = "glfw",
    srcs = ["libglfw.3.3.dylib"],
    hdrs = ["@lib-glfw//file"],
    strip_include_prefix = "/external/lib-glfw/file",
    include_prefix = "third_party/glfw",
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