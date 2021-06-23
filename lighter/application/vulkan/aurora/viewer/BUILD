package(default_visibility = ["//lighter/application/vulkan/aurora:__subpackages__"])

cc_library(
    name = "air_transmit_table",
    srcs = ["air_transmit_table.cc"],
    hdrs = ["air_transmit_table.h"],
    deps = ["//lighter/application/vulkan:common"],
)

cc_library(
    name = "distance_field",
    srcs = ["distance_field.cc"],
    hdrs = ["distance_field.h"],
    deps = ["//lighter/application/vulkan:common"],
)

cc_library(
    name = "path_dumper",
    srcs = ["path_dumper.cc"],
    hdrs = ["path_dumper.h"],
    deps = [
        ":distance_field",
        ":path_renderer",
        "//lighter/application/vulkan:common",
        "//third_party:absl",
    ],
)

cc_library(
    name = "path_renderer",
    srcs = ["path_renderer.cc"],
    hdrs = ["path_renderer.h"],
    deps = ["//lighter/application/vulkan:common"],
)

cc_library(
    name = "viewer",
    srcs = ["viewer.cc"],
    hdrs = ["viewer.h"],
    deps = [
        ":air_transmit_table",
        ":path_dumper",
        "//lighter/application/vulkan:common",
        "//lighter/application/vulkan/aurora:scene",
    ],
)
