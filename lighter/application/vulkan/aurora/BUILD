cc_library(
    name = "scene",
    hdrs = ["scene.h"],
    deps = ["//lighter/application/vulkan:common"],
    visibility = ["//lighter/application/vulkan/aurora:__subpackages__"],
)

cc_binary(
    name = "aurora",
    srcs = ["aurora.cc"],
    deps = [
        "//lighter/application/vulkan/aurora/editor",
        "//lighter/application/vulkan/aurora/viewer",
        "//lighter/application/vulkan:common",
    ],
    visibility = ["//visibility:private"],
)