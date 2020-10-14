package(default_visibility = ["//visibility:private"])

cc_library(
    name = "common",
    hdrs = ["util.h"],
    srcs = ["util.cc"],
    deps = [
        "//lighter/application:resource",
        "//lighter/application:util",
        "//lighter/common:camera",
        "//lighter/common:file",
        "//lighter/common:timer",
        "//lighter/common:util",
        "//lighter/renderer:onscreen_renderer",
        "//lighter/renderer/vulkan/extension:onscreen",
        "//third_party/absl/flags:flag",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:span",
        "//third_party:glm",
        "//third_party:vulkan",
    ],
    data = ["@resource//:resource"],
    visibility = ["//lighter/application/vulkan:__subpackages__"],
)

cc_binary(
    name = "cube",
    srcs = ["cube.cc"],
    deps = [":common"],
)

cc_library(
    name = "image_viewer",
    hdrs = ["image_viewer.h"],
    srcs = ["image_viewer.cc"],
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