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
        "//lighter/common:util",
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
    ],
    srcs = [
        "basic.cc",
        "context.cc",
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
    name = "pipeline",
    hdrs = ["pipeline.h"],
    srcs = ["pipeline.cc"],
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
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "property_checker",
    hdrs = ["property_checker.h"],
    srcs = ["property_checker.cc"],
    deps = [
        ":util",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "renderer",
    hdrs = ["renderer.h"],
    srcs = ["renderer.cc"],
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
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "render_pass",
    hdrs = ["render_pass.h"],
    srcs = ["render_pass.cc"],
    deps = [
        ":image",
        ":pipeline",
        ":type_mapping",
        ":util",
        "//lighter/common:util",
        "//lighter/renderer:pass",
        "//lighter/renderer:pipeline",
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
        "//third_party:vulkan"
    ],
)

cc_library(
    name = "type_mapping",
    hdrs = ["type_mapping.h"],
    srcs = ["type_mapping.cc"],
    deps = [
        ":util",
        "//lighter/common:util",
        "//lighter/renderer:type",
        "//third_party:vulkan",
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
