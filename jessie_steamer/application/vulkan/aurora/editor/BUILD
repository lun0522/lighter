package(default_visibility = ["//jessie_steamer/application/vulkan/aurora:__subpackages__"])

cc_library(
    name = "button",
    hdrs = ["button.h"],
    srcs = ["button.cc"],
    deps = [
        ":state",
        "//jessie_steamer/application/vulkan:common",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
    ],
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
        ":state",
        "//jessie_steamer/application/vulkan:common",
        "//jessie_steamer/common:rotation",
        "//third_party/absl/types:optional",
    ],
)

cc_library(
    name = "path",
    hdrs = ["path.h"],
    srcs = ["path.cc"],
    deps = [
        ":state",
        "//jessie_steamer/application/vulkan:common",
        "//jessie_steamer/common:spline",
        "//third_party/absl/types:optional",
    ],
)

cc_library(
    name = "state",
    hdrs = ["state.h"],
)