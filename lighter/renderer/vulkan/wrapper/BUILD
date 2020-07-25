package(default_visibility = ["//lighter:__subpackages__"])

load("//:settings.bzl", "compilation_mode")

compilation_mode()

cc_library(
    name = "basics",
    hdrs = [
        "basic_context.h",
        "basic_object.h",
    ] + select({
        ":optimal_build": [],
        "//conditions:default": ["validation.h"],
    }),
    srcs = ["basic_object.cc"] + select({
        ":optimal_build": [],
        "//conditions:default": ["validation.cc"],
    }),
    deps = [
        ":util",
        "//lighter/common:ref_count",
        "//lighter/common:util",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:span",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "buffer",
    hdrs = ["buffer.h"],
    srcs = ["buffer.cc"],
    deps = [
        ":basics",
        ":command",
        ":util",
        "//lighter/common:util",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:variant",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "command",
    hdrs = ["command.h"],
    srcs = ["command.cc"],
    deps = [
        ":basics",
        ":synchronization",
        ":util",
        "//lighter/common:util",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "descriptor",
    hdrs = ["descriptor.h"],
    srcs = ["descriptor.cc"],
    deps = [
        ":basics",
        ":util",
        "//lighter/common:model_loader",
        "//lighter/common:util",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/types:span",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "image",
    hdrs = [
        "image.h",
        "image_usage.h",
    ],
    srcs = [
        "image.cc",
        "image_usage.cc",
    ],
    deps = [
        ":basics",
        ":buffer",
        ":command",
        ":util",
        "//lighter/common:file",
        "//lighter/common:ref_count",
        "//lighter/common:util",
        "//third_party/absl/memory",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:span",
        "//third_party/absl/types:variant",
        "//third_party:vulkan",
    ]
)

cc_library(
    name = "pipeline",
    hdrs = [
        "pipeline.h",
        "pipeline_util.h",
    ],
    srcs = [
        "pipeline.cc",
        "pipeline_util.cc",
    ],
    deps = [
        ":basics",
        ":buffer",
        ":util",
        "//lighter/common:file",
        "//lighter/common:ref_count",
        "//lighter/common:util",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "render_pass",
    hdrs = [
        "render_pass.h",
        "render_pass_util.h",
    ],
    srcs = [
        "render_pass.cc",
        "render_pass_util.cc",
    ],
    deps = [
        ":basics",
        ":image",
        ":util",
        "//lighter/common:util",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:span",
        "//third_party/absl/types:variant",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "synchronization",
    hdrs = ["synchronization.h"],
    srcs = ["synchronization.cc"],
    deps = [
        ":basics",
        ":util",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    srcs = ["util.cc"],
    deps = [
        "//lighter/common:util",
        "//third_party/absl/container:flat_hash_set",
        "//third_party/absl/strings",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:span",
        "//third_party:glm",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "window",
    hdrs = [
        "swapchain.h",
        "window_context.h",
    ],
    srcs = ["swapchain.cc"],
    deps = [
        ":basics",
        ":image",
        ":util",
        "//lighter/common:window",
        "//lighter/common:util",
        "//third_party/absl/memory",
        "//third_party/absl/types:optional",
        "//third_party:vulkan",
    ],
)