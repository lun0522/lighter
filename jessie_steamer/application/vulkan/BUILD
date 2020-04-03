package(default_visibility = ["//visibility:private"])

cc_library(
    name = "common",
    hdrs = ["util.h"],
    srcs = ["util.cc"],
    deps = [
        "//jessie_steamer/application:resource",
        "//jessie_steamer/common:camera",
        "//jessie_steamer/common:file",
        "//jessie_steamer/common:timer",
        "//jessie_steamer/common:util",
        "//jessie_steamer/wrapper/vulkan:on_screen",
        "//third_party/absl/flags:flag",
        "//third_party/absl/memory",
        "//third_party:glm",
        "//third_party:vulkan",
    ],
    visibility = ["//jessie_steamer/application/vulkan:__subpackages__"],
)

cc_binary(
    name = "cube",
    srcs = ["cube.cc"],
    deps = [":common"],
)

cc_binary(
    name = "image_viewer",
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
    name = "triangle",
    srcs = ["triangle.cc"],
    deps = [":common"],
)