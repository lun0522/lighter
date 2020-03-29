package(default_visibility = ["//jessie_steamer/application/vulkan/aurora:__subpackages__"])

cc_library(
    name = "path_dumper",
    hdrs = ["path_dumper.h"],
    srcs = ["path_dumper.cc"],
    deps = ["//jessie_steamer/application/vulkan:common"],
)

cc_library(
    name = "viewer",
    hdrs = ["viewer.h"],
    srcs = ["viewer.cc"],
    deps = [
        ":path_dumper",
        "//jessie_steamer/application/vulkan/aurora:scene",
        "//jessie_steamer/application/vulkan:common",
    ],
)