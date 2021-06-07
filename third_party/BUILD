load("@//third_party:select_platform.bzl", "select_platform")

package(default_visibility = ["//visibility:public"])

# TODO: Review license.
licenses(["notice"])

cc_library(
    name = "absl",
    deps = [
        "@lib-absl//:absl",
        "@lib-absl-include//:absl_include",
    ],
)

cc_library(
    name = "assimp",
    deps = select({
        "@bazel_tools//src/conditions:windows": ["@lib-assimp-windows//:assimp"],
        "//conditions:default": ["@lib-assimp//:assimp"],
    }),
)

cc_library(
    name = "freetype",
    deps = select({
        "@bazel_tools//src/conditions:windows": ["@lib-freetype-windows//:freetype"],
        "//conditions:default": ["@lib-freetype//:freetype"],
    }),
)

cc_library(
    name = "glad",
    deps = ["@lib-glad//:glad"],
)

cc_library(
    name = "glfw",
    deps = select({
        "@bazel_tools//src/conditions:windows": ["@lib-glfw-windows//:glfw"],
        "//conditions:default": ["@lib-glfw//:glfw"],
    }),
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
    name = "picosha2",
    deps = ["@lib-picosha2//:picosha2"],
)

select_platform(
    name = "shaderc",
    linux_taregt = "@lib-shaderc-windows//:shaderc",
    osx_target = "@lib-shaderc-osx//:shaderc",
    windows_target = "@lib-shaderc-windows//:shaderc",
)

cc_library(
    name = "spirv_cross",
    deps = ["@lib-spirv-cross//:spirv_cross"],
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
