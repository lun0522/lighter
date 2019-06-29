package(default_visibility = ["//visibility:private"])

load("//:settings.bzl", "compilation_mode")

compilation_mode()

cc_library(
    name = "on_screen",
    deps = [
        ":buffer",
        ":command",
        ":image",
        ":model",
        ":render_pass",
        ":text",
        ":vertex_input_util",
        ":window",
    ],
    data = ["@resource//:resource"],
    visibility = ["//jessie_steamer:__subpackages__"],
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
        "//third_party:vulkan",
        "@lib-absl//absl/container:flat_hash_set",
        "@lib-absl//absl/types:optional",
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
        "//third_party:vulkan",
        "@lib-absl//absl/strings:str_format",
        "@lib-absl//absl/types:variant",
    ],
)

cc_library(
    name = "command",
    hdrs = ["command.h"],
    srcs = ["command.cc"],
    deps = [
        ":basics",
        ":synchronize",
        ":util",
        "//jessie_steamer/common:util",
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
        "//third_party:vulkan",
        "@lib-absl//absl/container:flat_hash_map",
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
        "//third_party:vulkan",
        "@lib-absl//absl/memory",
        "@lib-absl//absl/strings:str_format",
        "@lib-absl//absl/types:variant",
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
        ":vertex_input_util",
        "//jessie_steamer/common:file",
        "//jessie_steamer/common:model_loader",
        "//jessie_steamer/common:util",
        "//third_party:vulkan",
        "@lib-absl//absl/memory",
        "@lib-absl//absl/container:flat_hash_map",
        "@lib-absl//absl/strings:str_format",
        "@lib-absl//absl/types:optional",
        "@lib-absl//absl/types:variant",
    ],
)

cc_library(
    name = "pipeline",
    hdrs = ["pipeline.h"],
    srcs = ["pipeline.cc"],
    deps = [
        ":basics",
        ":render_pass",
        ":util",
        "//jessie_steamer/common:file",
        "//jessie_steamer/common:util",
        "//third_party:vulkan",
        "@lib-absl//absl/memory",
        "@lib-absl//absl/types:optional",
    ],
)

cc_library(
    name = "render_pass",
    hdrs = ["render_pass.h"],
    srcs = ["render_pass.cc"],
    deps = [
        ":basics",
        ":image",
        ":util",
        "//jessie_steamer/common:util",
        "//third_party:vulkan",
        "@lib-absl//absl/memory",
        "@lib-absl//absl/strings:str_format",
        "@lib-absl//absl/types:optional",
        "@lib-absl//absl/types:variant",
    ],
)

cc_library(
    name = "synchronize",
    hdrs = ["synchronize.h"],
    srcs = ["synchronize.cc"],
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
        ":basics",
        ":buffer",
        ":command",
        ":descriptor",
        ":image",
        ":pipeline",
        ":util",
        ":vertex_input_util",
        "//jessie_steamer/common:char_lib",
        "//jessie_steamer/common:util",
        "//third_party:glm",
        "@lib-absl//absl/container:flat_hash_map",
        "@lib-absl//absl/memory",
        "@lib-absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    deps = [
        "//jessie_steamer/common:util",
        "//third_party:vulkan",
        "@lib-absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "vertex_input_util",
    hdrs = ["vertex_input_util.h"],
    srcs = ["vertex_input_util.cc"],
    deps = [
        ":util",
        "//jessie_steamer/common:file",
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
        "//third_party:vulkan",
        "@lib-absl//absl/container:flat_hash_set",
        "@lib-absl//absl/memory",
    ],
)