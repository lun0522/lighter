package(default_visibility = ["//lighter:__subpackages__"])

cc_library(
    name = "buffer",
    srcs = ["buffer_util.cc"],
    hdrs = [
        "buffer.h",
        "buffer_util.h",
    ],
    deps = [
        ":type",
        "//lighter/common:file",
        "//third_party:absl",
    ],
)

cc_library(
    name = "buffer_usage",
    hdrs = ["buffer_usage.h"],
    deps = [
        ":type",
        "//lighter/common:util",
    ],
)

cc_library(
    name = "image",
    hdrs = ["image.h"],
    deps = [
        ":image_usage",
        ":type",
        "//lighter/common:image",
        "//third_party:glm",
    ],
)

cc_library(
    name = "image_usage",
    hdrs = ["image_usage.h"],
    deps = [
        ":type",
        "//lighter/common:util",
        "//third_party:absl",
    ],
)

cc_library(
    name = "pass",
    hdrs = [
        "pass.h",
        "pass_util.h",
    ],
    deps = [
        ":image",
        ":pipeline",
        ":type",
        "//lighter/common:util",
    ],
)

cc_library(
    name = "pipeline",
    srcs = ["pipeline_util.cc"],
    hdrs = [
        "pipeline.h",
        "pipeline_util.h",
    ],
    deps = [
        ":buffer",
        ":type",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:glm",
    ],
)

cc_library(
    name = "renderer",
    hdrs = ["renderer.h"],
    deps = [
        ":buffer",
        ":buffer_usage",
        ":image",
        ":image_usage",
        ":pass",
        ":pipeline",
        ":type",
        "//lighter/common:file",
        "//lighter/common:image",
        "//lighter/common:util",
        "//lighter/common:window",
        "//third_party:absl",
        "//third_party:glm",
    ],
)

cc_library(
    name = "type",
    hdrs = ["type.h"],
)
