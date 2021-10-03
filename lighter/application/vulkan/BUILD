package(default_visibility = ["//visibility:private"])

cc_library(
    name = "common",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    visibility = ["//lighter/application/vulkan:__subpackages__"],
    deps = [
        "//lighter/application:resource",
        "//lighter/common:camera",
        "//lighter/common:data",
        "//lighter/common:file",
        "//lighter/common:graphics_api",
        "//lighter/common:image",
        "//lighter/common:timer",
        "//lighter/common:util",
        "//lighter/renderer:util",
        "//lighter/renderer/vulkan/extension:onscreen",
        "//third_party:absl",
        "//third_party:glm",
        "//third_party:vulkan",
    ],
)

cc_binary(
    name = "cube",
    srcs = ["cube.cc"],
    deps = [":common"],
)

cc_library(
    name = "image_viewer",
    srcs = ["image_viewer.cc"],
    hdrs = ["image_viewer.h"],
    deps = [":common"],
)

cc_binary(
    name = "nanosuit",
    srcs = ["nanosuit.cc"],
    deps = [":common"],
)

cc_binary(
    name = "planet",
    srcs = ["planet.cc"],
    deps = [":common"],
)

cc_binary(
    name = "post_effect",
    srcs = ["post_effect.cc"],
    deps = [
        ":common",
        ":image_viewer",
    ],
)

cc_binary(
    name = "triangle",
    srcs = ["triangle.cc"],
    deps = [":common"],
)
