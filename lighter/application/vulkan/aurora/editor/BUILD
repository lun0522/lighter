package(default_visibility = ["//lighter/application/vulkan/aurora:__subpackages__"])

cc_library(
    name = "button",
    hdrs = ["button.h"],
    srcs = ["button.cc"],
    deps = [
        ":button_maker",
        ":button_util",
        "//lighter/application/vulkan:common",
        "//third_party:absl",
    ],
)

cc_library(
    name = "button_maker",
    hdrs = ["button_maker.h"],
    srcs = ["button_maker.cc"],
    deps = [
        ":button_util",
        "//lighter/application/vulkan:common",
        "//third_party:absl",
    ],
)

cc_library(
    name = "button_util",
    hdrs = ["button_util.h"],
    srcs = ["button_util.cc"],
    deps = ["//lighter/application/vulkan:common"],
)

cc_library(
    name = "celestial",
    hdrs = ["celestial.h"],
    srcs = ["celestial.cc"],
    deps = ["//lighter/application/vulkan:common"],
)

cc_library(
    name = "editor",
    hdrs = ["editor.h"],
    srcs = ["editor.cc"],
    deps = [
        ":button",
        ":celestial",
        ":path",
        "//lighter/application/vulkan/aurora:scene",
        "//lighter/application/vulkan:common",
        "//lighter/common:rotation",
        "//third_party:absl",
    ],
)

cc_library(
    name = "path",
    hdrs = ["path.h"],
    srcs = ["path.cc"],
    deps = [
        ":button_util",
        "//lighter/application/vulkan:common",
        "//lighter/common:spline",
        "//third_party:absl",
    ],
)