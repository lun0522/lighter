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
        "//third_party:absl",
    ],
)

cc_library(
    name = "image",
    hdrs = ["image.h"],
    deps = [
        ":type",
        ":image_usage",
    ],
)

cc_library(
    name = "image_usage",
    hdrs = ["image_usage.h"],
    srcs = ["image_usage.cc"],
    deps = [
        "//lighter/common:util",
        "//third_party:absl",
    ],
)

cc_library(
    name = "pass",
    hdrs = ["pass.h"],
    srcs = ["pass.cc"],
    deps = [
        ":image_usage",
        ":type",
        "//lighter/common:util",
        "//third_party:absl",
    ],
)

cc_library(
    name = "pipeline",
    hdrs = ["pipeline.h"],
    deps = [
        ":buffer",
        ":type",
        "//third_party:absl",
    ],
)

cc_library(
    name = "renderer",
    hdrs = ["renderer.h"],
    deps = [
        ":buffer",
        ":image",
        ":image_usage",
        ":pass",
        ":pipeline",
        "//lighter/common:file",
        "//lighter/common:image",
        "//third_party:absl",
    ],
)

cc_library(
    name = "type",
    hdrs = ["type.h"],
)
