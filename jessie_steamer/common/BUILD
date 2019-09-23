package(default_visibility = ["//jessie_steamer:__subpackages__"])

load("//:settings.bzl", "graphics_api")

graphics_api()

cc_library(
    name = "camera",
    srcs = ["camera.cc"],
    hdrs = ["camera.h"],
    deps = [
        ":util",
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
        "//third_party:freetype",
        "//third_party:glm",
        "@lib-absl//absl/container:flat_hash_map",
        "@lib-absl//absl/memory",
    ],
)

cc_library(
    name = "file",
    srcs = ["file.cc"],
    hdrs = ["file.h"],
    deps = [
        ":util",
        "//third_party:glm",
        "//third_party:stb",
        "@lib-absl//absl/container:flat_hash_map",
        "@lib-absl//absl/flags:flag",
        "@lib-absl//absl/memory",
        "@lib-absl//absl/strings",
        "@lib-absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "model_loader",
    srcs = ["model_loader.cc"],
    hdrs = ["model_loader.h"],
    deps = [
        ":file",
        ":util",
        "//third_party:assimp",
        "@lib-absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "ref_count",
    hdrs = ["ref_count.h"],
    deps = [
        "@lib-absl//absl/container:flat_hash_map",
        "@lib-absl//absl/memory",
    ],
)

cc_library(
    name = "time",
    hdrs = ["time.h"],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    deps = [
        "@lib-absl//absl/base",
        "@lib-absl//absl/flags:parse",
        "@lib-absl//absl/strings:str_format",
        "@lib-absl//absl/types:optional",
    ],
)

cc_library(
    name = "window",
    srcs = ["window.cc"],
    hdrs = ["window.h"],
    deps = [
        ":util",
        "//third_party:glm",
        "//third_party:glfw",
        "@lib-absl//absl/container:flat_hash_map",
    ] + select({
        "use_vulkan": ["//third_party:vulkan"],
        "//conditions:default": [],
    }),
)