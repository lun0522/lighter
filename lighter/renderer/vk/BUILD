package(default_visibility = ["//lighter:__subpackages__"])

cc_library(
    name = "context",
    hdrs = [
        "basic.h",
        "context.h",
        "debug_callback.h",
    ],
    srcs = [
        "basic.cc",
        "context.cc",
        "debug_callback.cc",
    ],
    deps = [
        ":util",
        "//lighter/common:util",
        "//lighter/common:window",
        "//lighter/renderer:type",
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "image",
    hdrs = ["image.h"],
    srcs = ["image.cc"],
    deps = [
        ":context",
        "//lighter/common:image",
        "//lighter/common:util",
        "//lighter/renderer:image",
        "//lighter/renderer:image_usage",
        "//lighter/renderer:type",
        "//third_party:absl",
    ],
)

cc_library(
    name = "renderer",
    hdrs = ["renderer.h"],
    srcs = ["renderer.cc"],
    deps = [
        ":context",
        ":image",
        "//lighter/common:image",
        "//lighter/renderer",
        "//lighter/renderer:image_usage",
        "//lighter/renderer:type",
        "//third_party:absl",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    deps = [
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)
