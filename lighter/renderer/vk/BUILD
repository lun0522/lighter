package(default_visibility = ["//lighter:__subpackages__"])

cc_library(
    name = "buffer",
    srcs = [
        "buffer.cc",
        "buffer_util.cc",
    ],
    hdrs = [
        "buffer.h",
        "buffer_util.h",
    ],
    deps = [
        ":context",
        ":util",
        "//lighter/common:util",
        "//lighter/renderer:buffer",
        "//lighter/renderer:buffer_usage",
        "//lighter/renderer:type",
        "//third_party:absl",
    ],
)

cc_library(
    name = "context",
    srcs = [
        "basic.cc",
        "context.cc",
    ],
    hdrs = [
        "basic.h",
        "context.h",
    ],
    deps = [
        ":property_checker",
        ":util",
        "//lighter/common:util",
        "//lighter/common:window",
        "//lighter/renderer:pipeline",
        "//lighter/renderer:type",
        "//lighter/renderer/vk:type_mapping",
        "//third_party:absl",
    ],
)

cc_library(
    name = "image",
    srcs = [
        "image.cc",
        "image_util.cc",
    ],
    hdrs = [
        "image.h",
        "image_util.h",
    ],
    deps = [
        ":buffer",
        ":context",
        ":type_mapping",
        ":util",
        "//lighter/common:image",
        "//lighter/common:util",
        "//lighter/renderer:image",
        "//lighter/renderer:image_usage",
        "//lighter/renderer:type",
        "//third_party:absl",
    ],
)

cc_test(
    name = "image_util_test",
    srcs = ["image_util_test.cc"],
    deps = [
        ":image",
        ":util",
        "//third_party:gtest",
    ],
)

cc_library(
    name = "pipeline",
    srcs = ["pipeline.cc"],
    hdrs = ["pipeline.h"],
    deps = [
        ":context",
        ":image",
        ":type_mapping",
        ":util",
        "//lighter/common:file",
        "//lighter/common:ref_count",
        "//lighter/common:util",
        "//lighter/renderer:image",
        "//lighter/renderer:image_usage",
        "//lighter/renderer:pass",
        "//lighter/renderer:pipeline",
        "//lighter/shader_compiler:util",
        "//third_party:absl",
    ],
)

cc_library(
    name = "property_checker",
    srcs = ["property_checker.cc"],
    hdrs = ["property_checker.h"],
    deps = [
        ":util",
        "//lighter/common:util",
        "//third_party:absl",
    ],
)

cc_library(
    name = "renderer",
    srcs = ["renderer.cc"],
    hdrs = ["renderer.h"],
    deps = [
        ":buffer",
        ":context",
        ":image",
        ":pipeline",
        ":render_pass",
        ":swapchain",
        ":util",
        "//lighter/common:image",
        "//lighter/common:window",
        "//lighter/renderer",
        "//lighter/renderer:buffer_usage",
        "//lighter/renderer:image_usage",
        "//lighter/renderer:type",
        "//third_party:absl",
        "//third_party:glm",
    ],
)

cc_library(
    name = "render_pass",
    srcs = ["render_pass.cc"],
    hdrs = ["render_pass.h"],
    deps = [
        ":image",
        ":pipeline",
        ":type_mapping",
        ":util",
        "//lighter/common:util",
        "//lighter/renderer:pass",
        "//lighter/renderer:pipeline",
        "//third_party:absl",
    ],
)

cc_library(
    name = "swapchain",
    srcs = ["swapchain.cc"],
    hdrs = ["swapchain.h"],
    deps = [
        ":context",
        ":image",
        ":util",
        "//lighter/common:image",
        "//lighter/common:util",
        "//lighter/renderer:image_usage",
        "//lighter/renderer:type",
        "//third_party:absl",
    ],
)

cc_library(
    name = "type_mapping",
    srcs = ["type_mapping.cc"],
    hdrs = ["type_mapping.h"],
    deps = [
        ":util",
        "//lighter/common:util",
        "//lighter/renderer:type",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    deps = [
        "//lighter/common:image",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:glm",
        "//third_party:vulkan",
    ],
)
