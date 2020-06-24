package(default_visibility = ["//visibility:private"])

cc_library(
    name = "geometry_pass",
    hdrs = ["geometry_pass.h"],
    srcs = ["geometry_pass.cc"],
    deps = ["//lighter/application/vulkan:common"],
)

cc_library(
    name = "lighting_pass",
    hdrs = ["lighting_pass.h"],
    srcs = ["lighting_pass.cc"],
    deps = ["//lighter/application/vulkan:common"],
)

cc_binary(
    name = "troop",
    srcs = ["troop.cc"],
    deps = [
        ":geometry_pass",
        ":lighting_pass",
        "//lighter/application/vulkan:common",
    ],
)