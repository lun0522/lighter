package(default_visibility = ["//visibility:private"])

cc_library(
    name = "geometry_pass",
    srcs = ["geometry_pass.cc"],
    hdrs = ["geometry_pass.h"],
    deps = ["//lighter/application/vulkan:common"],
)

cc_library(
    name = "lighting_pass",
    srcs = ["lighting_pass.cc"],
    hdrs = ["lighting_pass.h"],
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
