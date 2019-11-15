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
    name = "button",
    hdrs = ["button.h"],
    srcs = ["button.cc"],
    deps = [
        "//jessie_steamer/application/vulkan:common",
        "//third_party/absl/strings:str_format",
    ],
)

cc_library(
    name = "editor",
    hdrs = ["editor.h"],
    srcs = ["editor.cc"],
    deps = [
        ":button",
        "//jessie_steamer/application/vulkan:common",
        "//jessie_steamer/common:rotation",
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