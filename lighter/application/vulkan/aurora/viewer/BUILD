package(default_visibility = ["//lighter/application/vulkan/aurora:__subpackages__"])

cc_library(
    name = "air_transmit_table",
    srcs = ["air_transmit_table.cc"],
    hdrs = ["air_transmit_table.h"],
    deps = ["//lighter/application/vulkan:common"],
)

cc_library(
    name = "distance_field",
    hdrs = ["distance_field.h"],
    srcs = ["distance_field.cc"],
    deps = ["//lighter/application/vulkan:common"],
)

cc_library(
    name = "path_dumper",
    hdrs = ["path_dumper.h"],
    srcs = ["path_dumper.cc"],
    deps = [
        ":distance_field",
        ":path_renderer",
        "//lighter/application/vulkan:common",
        "//third_party/absl/strings:str_format",
    ],
)

cc_library(
    name = "path_renderer",
    hdrs = ["path_renderer.h"],
    srcs = ["path_renderer.cc"],
    deps = ["//lighter/application/vulkan:common"],
)

cc_library(
    name = "viewer",
    hdrs = ["viewer.h"],
    srcs = ["viewer.cc"],
    deps = [
        ":air_transmit_table",
        ":path_dumper",
        "//lighter/application/vulkan/aurora:scene",
        "//lighter/application/vulkan:common",
    ],
)