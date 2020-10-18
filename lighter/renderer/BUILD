package(default_visibility = ["//lighter:__subpackages__"])

cc_library(
    name = "align",
    hdrs = ["align.h"],
)

cc_library(
    name = "buffer",
    hdrs = ["buffer.h"],
    deps = [
        ":type",
        "//third_party/absl/types:span",
    ],
)

cc_library(
    name = "image_usage",
    hdrs = ["image_usage.h"],
    srcs = ["image_usage.cc"],
    deps = [
        "//lighter/common:util",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/strings",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:span",
    ],
)

cc_library(
    name = "offscreen_renderer",
    deps = [
        ":align",
        ":buffer",
        ":image_usage",
        ":pass",
        ":pipeline",
        ":renderer",
        ":type",
    ],
)

cc_library(
    name = "onscreen_renderer",
    deps = [":offscreen_renderer"],
)

cc_library(
    name = "pass",
    hdrs = ["pass.h"],
    srcs = ["pass.cc"],
    deps = [
        ":image_usage",
        ":type",
        "//lighter/common:util",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/strings",
        "//third_party/absl/strings:str_format",
    ],
)

cc_library(
    name = "pipeline",
    hdrs = ["pipeline.h"],
    deps = [
        ":buffer",
        ":type",
        "//third_party/absl/strings",
    ],
)

cc_library(
    name = "renderer",
    hdrs = ["renderer.h"],
    deps = [
        ":buffer",
        ":pass",
        ":pipeline",
        "//third_party/absl/types:span",
    ],
)

cc_library(
    name = "type",
    hdrs = ["type.h"],
)
