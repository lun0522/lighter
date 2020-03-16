cc_library(
    name = "scene",
    hdrs = ["scene.h"],
    deps = ["//jessie_steamer/application/vulkan:common"],
    visibility = ["//jessie_steamer/application/vulkan/aurora:__subpackages__"],
)

cc_binary(
    name = "aurora",
    srcs = ["aurora.cc"],
    deps = [
        "//jessie_steamer/application/vulkan/aurora/editor",
        "//jessie_steamer/application/vulkan/aurora/viewer",
        "//jessie_steamer/application/vulkan:common",
    ],
    visibility = ["//visibility:private"],
)