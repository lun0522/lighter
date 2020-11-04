package(default_visibility = ["//lighter:__subpackages__"])

cc_library(
    name = "buffer",
    hdrs = [
        "buffer.h",
        "buffer_util.h",
    ],
    srcs = [
        "buffer.cc",
        "buffer_util.cc",
    ],
    deps = [
        ":context",
        ":util",
        "//lighter/renderer:buffer",
        "//lighter/renderer:buffer_usage",
        "//lighter/renderer:type",
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)

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
    hdrs = [
        "image.h",
        "image_util.h",
    ],
    srcs = [
        "image.cc",
        "image_util.cc",
    ],
    deps = [
        ":context",
        ":util",
        "//lighter/common:image",
        "//lighter/common:util",
        "//lighter/renderer:image",
        "//lighter/renderer:image_usage",
        "//lighter/renderer:type",
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)

cc_test(
    name = "image_util_test",
    srcs = ["image_util_test.cc"],
    deps = [
        ":image",
        "//third_party:gtest_main",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "renderer",
    hdrs = ["renderer.h"],
    srcs = ["renderer.cc"],
    deps = [
        ":context",
        ":image",
        ":swapchain",
        ":util",
        "//lighter/common:image",
        "//lighter/common:window",
        "//lighter/renderer",
        "//lighter/renderer:image_usage",
        "//lighter/renderer:type",
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "swapchain",
    hdrs = ["swapchain.h"],
    srcs = ["swapchain.cc"],
    deps = [
        ":context",
        ":image",
        ":util",
        "//lighter/common:image",
        "//lighter/common:util",
        "//lighter/renderer:image_usage",
        "//lighter/renderer:type",
        "//third_party:absl",
        "//third_party:glm",
        "//third_party:vulkan"
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    deps = [
        "//lighter/common:image",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)
