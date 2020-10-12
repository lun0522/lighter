package(default_visibility = ["//lighter:__subpackages__"])

cc_library(
    name = "align",
    hdrs = ["align.h"],
)

cc_library(
    name = "image_usage",
    hdrs = ["image_usage.h"],
    srcs = ["image_usage.cc"],
    deps = [
        "//lighter/common:util",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:span",
    ],
)

cc_library(
    name = "offscreen_renderer",
    deps = [
        ":align",
        ":image_usage",
    ],
    data = ["@resource//:resource"],
)
