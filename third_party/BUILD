package(default_visibility = ["//visibility:public"])

# TODO: Review license.
licenses(["notice"])

cc_library(
    name = "absl",
    deps = [
        "@lib-absl//absl/base:core_headers",
        "@lib-absl//absl/container:flat_hash_map",
        "@lib-absl//absl/container:flat_hash_set",
        "@lib-absl//absl/flags:flag",
        "@lib-absl//absl/flags:parse",
        "@lib-absl//absl/functional:function_ref",
        "@lib-absl//absl/strings",
        "@lib-absl//absl/strings:str_format",
        "@lib-absl//absl/types:span",
        "@lib-absl-include//:absl_include",
    ],
)

cc_library(
    name = "assimp",
    deps = select({
        "@platforms//os:windows": ["@lib-assimp-windows//:assimp"],
        "//conditions:default": ["@lib-assimp//:assimp"],
    }),
)

cc_library(
    name = "freetype",
    deps = select({
        "@platforms//os:windows": ["@lib-freetype-windows//:freetype"],
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
        "@platforms//os:windows": ["@lib-glfw-windows//:glfw"],
        "//conditions:default": ["@lib-glfw//:glfw"],
    }),
)

cc_library(
    name = "gtest",
    deps = [
        "@lib-gtest//:gtest_main",
        "@lib-gtest-include//:gtest_include",
    ],
)

cc_library(
    name = "glm",
    deps = ["@lib-glm//:glm"],
)

cc_library(
    name = "picosha2",
    deps = ["@lib-picosha2//:picosha2"],
)

cc_library(
    name = "shaderc",
    deps = ["@lib-shaderc//:shaderc"],
)

cc_library(
    name = "spirv_cross",
    deps = select({
        "@platforms//os:windows": ["@lib-spirv-cross-windows//:spirv_cross"],
        "//conditions:default": ["@lib-spirv-cross//:spirv_cross"],
    }),
)

cc_library(
    name = "stb",
    deps = ["@lib-stb//:stb"],
)

cc_library(
    name = "vulkan",
    deps = ["@lib-vulkan//:vulkan"],
)
