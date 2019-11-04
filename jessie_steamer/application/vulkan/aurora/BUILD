package(default_visibility = ["//visibility:private"])

cc_binary(
    name = "aurora",
    srcs = ["aurora.cc"],
    deps = [
        ":editor",
        ":viewer",
        "//jessie_steamer/application/vulkan:common",
    ],
)

cc_library(
    name = "editor",
    hdrs = ["editor.h"],
    srcs = ["editor.cc"],
    deps = [
        "//jessie_steamer/application/vulkan:common",
        "//third_party/absl/types:optional",
    ],
)

cc_library(
    name = "viewer",
    hdrs = ["viewer.h"],
    srcs = ["viewer.cc"],
    deps = [
        "//jessie_steamer/application/vulkan:common",
    ],
)