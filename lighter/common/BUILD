load("//:settings.bzl", "graphics_api")

package(default_visibility = ["//lighter:__subpackages__"])

graphics_api()

cc_library(
    name = "camera",
    srcs = ["camera.cc"],
    hdrs = ["camera.h"],
    deps = [
        ":util",
        "//third_party:absl",
        "//third_party:glm",
    ],
)

cc_library(
    name = "char_lib",
    srcs = ["char_lib.cc"],
    hdrs = ["char_lib.h"],
    deps = [
        ":file",
        ":image",
        ":util",
        "//third_party:absl",
        "//third_party:freetype",
        "//third_party:glm",
    ],
)

cc_library(
    name = "data",
    hdrs = ["data.h"],
    srcs = ["data.cc"],
    deps = [
        ":util",
        "//third_party:absl",
        "//third_party:glm",
    ],
)

cc_library(
    name = "file",
    srcs = ["file.cc"],
    hdrs = ["file.h"],
    deps = [
        ":data",
        ":graphics_api",
        ":util",
        "//lighter/shader_compiler:util",
        "//third_party:absl",
        "//third_party:glm",
        "@bazel_tools//tools/cpp/runfiles",
    ],
)

cc_library(
    name = "graphics_api",
    srcs = ["graphics_api.cc"],
    hdrs = ["graphics_api.h"],
)

cc_library(
    name = "image",
    srcs = ["image.cc"],
    hdrs = ["image.h"],
    deps = [
        ":data",
        ":file",
        ":util",
        "//third_party:absl",
        "//third_party:glm",
        "//third_party:stb",
    ],
)

cc_library(
    name = "model_loader",
    srcs = ["model_loader.cc"],
    hdrs = ["model_loader.h"],
    deps = [
        ":file",
        ":util",
        "//third_party:absl",
        "//third_party:assimp",
    ],
)

cc_library(
    name = "ref_count",
    hdrs = ["ref_count.h"],
    deps = [
        ":util",
        "//third_party:absl",
    ],
)

cc_library(
    name = "rotation",
    srcs = ["rotation.cc"],
    hdrs = ["rotation.h"],
    deps = [
        ":camera",
        ":timer",
        ":util",
        "//third_party:glm",
    ],
)

cc_library(
    name = "spline",
    srcs = ["spline.cc"],
    hdrs = ["spline.h"],
    deps = [
        ":util",
        "//third_party:absl",
        "//third_party:glm",
    ],
)

cc_library(
    name = "timer",
    hdrs = ["timer.h"],
)

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    deps = ["//third_party:absl"],
)

cc_library(
    name = "window",
    srcs = ["window.cc"],
    hdrs = ["window.h"],
    deps = [
        ":util",
        "//third_party:absl",
        "//third_party:glm",
        "//third_party:glfw",
    ] + select({
        "use_vulkan": ["//third_party:vulkan"],
        "//conditions:default": [],
    }),
)
