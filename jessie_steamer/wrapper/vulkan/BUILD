package(default_visibility = ["//visibility:private"])

cc_library(
    name = "on_screen",
    deps = [
        ":command",
        ":window",
    ],
    data = ["@resource//:resource"],
    visibility = ["//jessie_steamer:__subpackages__"],
)

cc_library(
    name = "vulkan_wrapper",
    srcs = [
        "model.cc",
        "pipeline.cc",
        "render_pass.cc",
    ],
    hdrs = [
        "model.h",
        "pipeline.h",
        "render_pass.h",
    ],
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
        "//third_party:vulkan",
        "@lib-absl//absl/strings:str_format",
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

config_setting(
    name = "optimal_build",
    values = {"compilation_mode": "opt"},
)