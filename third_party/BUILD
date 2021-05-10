package(default_visibility = ["//visibility:public"])

licenses(["notice"])

load("@//third_party:select_platform.bzl", "select_platform")

cc_library(
    name = "absl",
    deps = [
        "@lib-absl//:absl",
        "@lib-absl-include//:absl_include",
    ],
)

select_platform(
    name = "assimp",
    include_target = "@lib-assimp-include//:assimp_include",
    linux_taregt = "@lib-assimp-ubuntu//:assimp_lib",
    osx_target = "@lib-assimp-osx//:assimp_lib",
    windows_target = "@lib-assimp-windows//:assimp_lib",
)

select_platform(
    name = "freetype",
    include_target = "@lib-freetype-include//:freetype_include",
    linux_taregt = "@lib-freetype-ubuntu//:freetype_lib",
    osx_target = "@lib-freetype-osx//:freetype_lib",
    windows_target = "@lib-freetype-windows//:freetype_lib",
)

cc_library(
    name = "glad",
    deps = ["@lib-glad//:glad"],
)

select_platform(
    name = "glfw",
    include_target = "@lib-glfw-include//:glfw_include",
    linux_taregt = "@lib-glfw-ubuntu//:glfw_lib",
    osx_target = "@lib-glfw-osx//:glfw_lib",
    windows_target = "@lib-glfw-windows//:glfw_lib",
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
    name = "spirv_cross",
    deps = select({
        "@bazel_tools//src/conditions:darwin": [
            "@lib-spirv-cross-include-osx//:spirv_cross_include",
            "@lib-spirv-cross-lib-osx//:spirv_cross_lib",
        ],
        "@bazel_tools//src/conditions:linux_x86_64": [
            "@lib-spirv-cross-include-linux//:spirv_cross_include",
            "@lib-spirv-cross-lib-linux//:spirv_cross_lib",
        ],
        "@bazel_tools//src/conditions:windows": [
            "@lib-spirv-cross-include-windows//:spirv_cross_include",
            "@lib-spirv-cross-lib-windows//:spirv_cross_lib",
        ],
    }),
)

cc_library(
    name = "stb",
    deps = ["@lib-stb//:stb"],
)

select_platform(
    name = "vulkan",
    linux_taregt = "@lib-vulkan-linux//:vulkan",
    osx_target = "@lib-vulkan-osx//:vulkan",
    windows_target = "@lib-vulkan-windows//:vulkan",
)
