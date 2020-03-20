package(default_visibility = ["//jessie_steamer/application/vulkan/aurora:__subpackages__"])

cc_library(
    name = "button",
    hdrs = ["button.h"],
    srcs = ["button.cc"],
    deps = [
        ":button_maker",
        ":button_util",
        "//jessie_steamer/application/vulkan:common",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:span",
    ],
)

cc_library(
    name = "button_maker",
    hdrs = ["button_maker.h"],
    srcs = ["button_maker.cc"],
    deps = [
        ":button_util",
        "//jessie_steamer/application/vulkan:common",
        "//third_party/absl/types:span",
    ],
)

cc_library(
    name = "button_util",
    hdrs = ["button_util.h"],
    srcs = ["button_util.cc"],
    deps = ["//jessie_steamer/application/vulkan:common"],
)

cc_library(
    name = "celestial",
    hdrs = ["celestial.h"],
    srcs = ["celestial.cc"],
    deps = ["//jessie_steamer/application/vulkan:common"],
)

cc_library(
    name = "editor",
    hdrs = ["editor.h"],
    srcs = ["editor.cc"],
    deps = [
        ":button",
        ":celestial",
        ":path",
        "//jessie_steamer/application/vulkan/aurora:scene",
        "//jessie_steamer/application/vulkan:common",
        "//jessie_steamer/common:rotation",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:span",
    ],
)

cc_library(
    name = "path",
    hdrs = ["path.h"],
    srcs = ["path.cc"],
    deps = [
        ":button_util",
        "//jessie_steamer/application/vulkan:common",
        "//jessie_steamer/common:spline",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:span",
    ],
)