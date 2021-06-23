cc_library(
    name = "scene",
    hdrs = ["scene.h"],
    visibility = ["//lighter/application/vulkan/aurora:__subpackages__"],
    deps = ["//lighter/application/vulkan:common"],
)

cc_binary(
    name = "aurora",
    srcs = ["aurora.cc"],
    visibility = ["//visibility:private"],
    deps = [
        "//lighter/application/vulkan:common",
        "//lighter/application/vulkan/aurora/editor",
        "//lighter/application/vulkan/aurora/viewer",
    ],
)
