package(default_visibility = ["//visibility:public"])

licenses(["notice"])

load("@//third_party:select_platform.bzl", "select_platform")

cc_library(
    name = "absl",
    deps = [
        "@lib-absl//:absl",
        "@lib-absl-headers//:absl_headers",
    ],
)

select_platform(
    name = "assimp",
    headers_target = "@lib-assimp-headers//:assimp_headers",
    linux_taregt = "@lib-assimp-ubuntu//:assimp",
    osx_target = "@lib-assimp-osx//:assimp",
)

select_platform(
    name = "freetype",
    headers_target = "@lib-freetype-headers//:freetype_headers",
    linux_taregt = "@lib-freetype-ubuntu//:freetype",
    osx_target = "@lib-freetype-osx//:freetype",
)

cc_library(
    name = "glad",
    deps = ["@lib-glad//:glad"],
)

select_platform(
    name = "glfw",
    headers_target = "@lib-glfw-headers//:glfw_headers",
    linux_taregt = "@lib-glfw-ubuntu//:glfw",
    osx_target = "@lib-glfw-osx//:glfw",
)

cc_library(
    name = "gtest_main",
    deps = ["@lib-googletest//:gtest_main"],
)

cc_library(
    name = "glm",
    deps = ["@lib-glm//:glm"],
)

cc_library(
    name = "stb",
    deps = ["@lib-stb//:stb"],
)

select_platform(
    name = "vulkan",
    linux_taregt = "@lib-vulkan-linux//:vulkan",
    osx_target = "@lib-vulkan-osx//:vulkan",
)
