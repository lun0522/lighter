load("//:settings.bzl", "compilation_mode")

package(default_visibility = ["//lighter:__subpackages__"])

compilation_mode()

cc_library(
    name = "basics",
    srcs = ["basic_object.cc"] + select({
        ":optimal_build": [],
        "//conditions:default": ["validation.cc"],
    }),
    hdrs = [
        "basic_context.h",
        "basic_object.h",
    ] + select({
        ":optimal_build": [],
        "//conditions:default": ["validation.h"],
    }),
    deps = [
        ":util",
        "//lighter/common:ref_count",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "buffer",
    srcs = ["buffer.cc"],
    hdrs = ["buffer.h"],
    deps = [
        ":basics",
        ":command",
        ":util",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "command",
    srcs = ["command.cc"],
    hdrs = ["command.h"],
    deps = [
        ":basics",
        ":synchronization",
        ":util",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "descriptor",
    srcs = ["descriptor.cc"],
    hdrs = ["descriptor.h"],
    deps = [
        ":basics",
        ":util",
        "//lighter/common:model_loader",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:vulkan",
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
        ":basics",
        ":buffer",
        ":command",
        ":util",
        "//lighter/common:file",
        "//lighter/common:image",
        "//lighter/common:ref_count",
        "//lighter/common:util",
        "//lighter/renderer/ir:image_usage",
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)

cc_test(
    name = "image_util_test",
    srcs = ["image_util_test.cc"],
    deps = [
        ":image",
        "//third_party:gtest",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "pipeline",
    srcs = [
        "pipeline.cc",
        "pipeline_util.cc",
    ],
    hdrs = [
        "pipeline.h",
        "pipeline_util.h",
    ],
    deps = [
        ":basics",
        ":buffer",
        ":util",
        "//lighter/common:data",
        "//lighter/common:file",
        "//lighter/common:ref_count",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "render_pass",
    srcs = ["render_pass.cc"],
    hdrs = ["render_pass.h"],
    deps = [
        ":basics",
        ":image",
        ":util",
        "//lighter/common:image",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "synchronization",
    srcs = ["synchronization.cc"],
    hdrs = ["synchronization.h"],
    deps = [
        ":basics",
        ":util",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    deps = [
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:glm",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "window",
    srcs = ["swapchain.cc"],
    hdrs = [
        "swapchain.h",
        "window_context.h",
    ],
    deps = [
        ":basics",
        ":image",
        ":util",
        "//lighter/common:image",
        "//lighter/common:util",
        "//lighter/common:window",
        "//third_party:vulkan",
    ],
)
