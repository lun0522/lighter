package(default_visibility = ["//lighter:__subpackages__"])

cc_library(
    name = "context",
    hdrs = ["context.h"],
    deps = [],
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
        "//third_party/absl/types:span",
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
        "//third_party/absl/memory",
        "//third_party/absl/types:span",
    ],
)
