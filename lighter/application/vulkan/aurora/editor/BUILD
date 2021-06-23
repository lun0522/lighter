package(default_visibility = ["//lighter/application/vulkan/aurora:__subpackages__"])

cc_library(
    name = "button",
    srcs = ["button.cc"],
    hdrs = ["button.h"],
    deps = [
        ":button_maker",
        ":button_util",
        "//lighter/application/vulkan:common",
        "//third_party:absl",
    ],
)

cc_library(
    name = "button_maker",
    srcs = ["button_maker.cc"],
    hdrs = ["button_maker.h"],
    deps = [
        ":button_util",
        "//lighter/application/vulkan:common",
        "//third_party:absl",
    ],
)

cc_library(
    name = "button_util",
    srcs = ["button_util.cc"],
    hdrs = ["button_util.h"],
    deps = ["//lighter/application/vulkan:common"],
)

cc_library(
    name = "celestial",
    srcs = ["celestial.cc"],
    hdrs = ["celestial.h"],
    deps = ["//lighter/application/vulkan:common"],
)

cc_library(
    name = "editor",
    srcs = ["editor.cc"],
    hdrs = ["editor.h"],
    deps = [
        ":button",
        ":celestial",
        ":path",
        "//lighter/application/vulkan:common",
        "//lighter/application/vulkan/aurora:scene",
        "//lighter/common:rotation",
        "//third_party:absl",
    ],
)

cc_library(
    name = "path",
    srcs = ["path.cc"],
    hdrs = ["path.h"],
    deps = [
        ":button_util",
        "//lighter/application/vulkan:common",
        "//lighter/common:spline",
        "//third_party:absl",
    ],
)
