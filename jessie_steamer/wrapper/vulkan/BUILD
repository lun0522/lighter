package(default_visibility = ["//visibility:private"])

load("//:settings.bzl", "compilation_mode")

compilation_mode()

cc_library(
    name = "on_screen",
    deps = [
        ":align",
        ":buffer",
        ":command",
        ":image",
        ":model",
        ":pipeline",
        ":render_pass",
        ":text",
        ":window",
    ],
    data = ["@resource//:resource"],
    visibility = ["//jessie_steamer:__subpackages__"],
)

cc_library(
    name = "align",
    hdrs = ["align.h"],
)

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
        "//jessie_steamer/common:util",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/absl/types:optional",
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
        "//jessie_steamer/common:util",
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
        "//jessie_steamer/common:util",
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
        "//jessie_steamer/common:model_loader",
        "//jessie_steamer/common:util",
        "//third_party/absl/container:flat_hash_map",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "image",
    hdrs = ["image.h"],
    srcs = ["image.cc"],
    deps = [
        ":basics",
        ":buffer",
        ":util",
        "//jessie_steamer/common:file",
        "//jessie_steamer/common:ref_count",
        "//jessie_steamer/common:util",
        "//third_party/absl/memory",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:variant",
        "//third_party:vulkan",
    ]
)

cc_library(
    name = "model",
    hdrs = ["model.h"],
    srcs = ["model.cc"],
    deps = [
        ":basics",
        ":buffer",
        ":descriptor",
        ":image",
        ":pipeline",
        ":render_pass",
        ":util",
        "//jessie_steamer/common:file",
        "//jessie_steamer/common:model_loader",
        "//jessie_steamer/common:util",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/memory",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:variant",
        "//third_party:vulkan",
    ],
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
        "//jessie_steamer/common:file",
        "//jessie_steamer/common:ref_count",
        "//jessie_steamer/common:util",
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
        "//jessie_steamer/common:util",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
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
    name = "text",
    hdrs = [
        "text.h",
        "text_util.h"
    ],
    srcs = [
        "text.cc",
        "text_util.cc",
    ],
    deps = [
        ":align",
        ":basics",
        ":buffer",
        ":command",
        ":descriptor",
        ":image",
        ":pipeline",
        ":render_pass",
        ":util",
        "//jessie_steamer/common:char_lib",
        "//jessie_steamer/common:file",
        "//jessie_steamer/common:util",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/memory",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party:glm",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    deps = [
        "//jessie_steamer/common:util",
        "//third_party/absl/container:flat_hash_set",
        "//third_party/absl/strings",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
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
        "//jessie_steamer/common:window",
        "//jessie_steamer/common:util",
        "//third_party/absl/memory",
        "//third_party/absl/types:optional",
        "//third_party:vulkan",
    ],
)