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
        ":macro",
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
        ":macro",
        "//jessie_steamer/common:util",
        "//third_party:vulkan",
        "@lib-absl//absl/strings:str_format",
        "@lib-absl//absl/types:span",
    ],
)

cc_library(
    name = "command",
    hdrs = ["command.h"],
    srcs = ["command.cc"],
    deps = [
        ":basics",
        ":macro",
        ":synchronize",
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
        ":macro",
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
        ":macro",
        "//jessie_steamer/common:file",
        "//jessie_steamer/common:ref_count",
        "//jessie_steamer/common:util",
        "//third_party:vulkan",
        "@lib-absl//absl/memory",
        "@lib-absl//absl/strings:str_format",
        "@lib-absl//absl/types:span",
        "@lib-absl//absl/types:variant",
    ]
)

cc_library(
    name = "macro",
    hdrs = ["macro.h"],
    deps = [
        "//jessie_steamer/common:util",
        "//third_party:vulkan",
        "@lib-absl//absl/strings:str_format",
    ],
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
        ":macro",
        ":pipeline",
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
        ":macro",
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
        ":macro",
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
        ":macro",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "text",
    hdrs = ["text.h"],
    srcs = ["text.cc"],
    deps = [
        ":image",
        "//jessie_steamer/common:char_lib",
        "//third_party:glm",
        "@lib-absl//absl/container:flat_hash_map",
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
        ":macro",
        "//jessie_steamer/common:window",
        "//jessie_steamer/common:util",
        "//third_party:vulkan",
        "@lib-absl//absl/container:flat_hash_set",
        "@lib-absl//absl/memory",
    ],
)