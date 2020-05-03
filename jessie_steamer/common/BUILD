package(default_visibility = ["//jessie_steamer:__subpackages__"])

load("//:settings.bzl", "graphics_api")

graphics_api()

cc_library(
    name = "camera",
    srcs = ["camera.cc"],
    hdrs = ["camera.h"],
    deps = [
        ":util",
        "//third_party/absl/types:optional",
        "//third_party:glm",
    ],
)

cc_library(
    name = "char_lib",
    srcs = ["char_lib.cc"],
    hdrs = ["char_lib.h"],
    deps = [
        ":file",
        ":util",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/memory",
        "//third_party/absl/types:span",
        "//third_party:freetype",
        "//third_party:glm",
    ],
)

cc_library(
    name = "file",
    srcs = ["file.cc"],
    hdrs = ["file.h"],
    deps = [
        ":util",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/flags:flag",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/absl/strings:str_format",
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
        "//third_party/absl/strings:str_format",
        "//third_party:assimp",
    ],
)

cc_library(
    name = "ref_count",
    hdrs = ["ref_count.h"],
    deps = [
        ":util",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/memory",
    ],
)

cc_library(
    name = "rotation",
    hdrs = ["rotation.h"],
    srcs = ["rotation.cc"],
    deps = [
        ":camera",
        ":timer",
        ":util",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:variant",
        "//third_party:glm",
    ],
)

cc_library(
    name = "spline",
    hdrs = ["spline.h"],
    srcs = ["spline.cc"],
    deps = [
        ":util",
        "//third_party/absl/memory",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:span",
        "//third_party:glm",
    ],
)

cc_library(
    name = "timer",
    hdrs = ["timer.h"],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    srcs = ["util.cc"],
    deps = [
        "//third_party/absl/base:core_headers",
        "//third_party/absl/flags:parse",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:span",
    ],
)

cc_library(
    name = "window",
    srcs = ["window.cc"],
    hdrs = ["window.h"],
    deps = [
        ":util",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/strings:str_format",
        "//third_party:glm",
        "//third_party:glfw",
    ] + select({
        "use_vulkan": ["//third_party:vulkan"],
        "//conditions:default": [],
    }),
)