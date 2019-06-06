package(default_visibility = ["//visibility:private"])

cc_library(
    name = "vulkan_wrapper",
    srcs = [
        "buffer.cc",
        "descriptor.cc",
        "image.cc",
        "model.cc",
        "pipeline.cc",
        "render_pass.cc",
        "swapchain.cc",
        "text.cc",
    ],
    hdrs = [
        "buffer.h",
        "descriptor.h",
        "image.h",
        "model.h",
        "pipeline.h",
        "render_pass.h",
        "swapchain.h",
        "text.h",
    ],
    deps = [
        ":command",
        ":macro",
        ":synchronize",
        "//jessie_steamer/common:char_lib",
        "//jessie_steamer/common:file",
        "//jessie_steamer/common:model_loader",
        "//jessie_steamer/common:ref_count",
        "//jessie_steamer/common:util",
        "//jessie_steamer/common:window",
        "//third_party:glm",
        "//third_party:vulkan",
        "@lib-absl//absl/container:flat_hash_map",
        "@lib-absl//absl/container:flat_hash_set",
        "@lib-absl//absl/container:node_hash_map",
        "@lib-absl//absl/memory",
        "@lib-absl//absl/strings:str_format",
        "@lib-absl//absl/types:optional",
        "@lib-absl//absl/types:span",
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
    name = "command",
    hdrs = ["command.h"],
    srcs = ["command.cc"],
    deps = [
        ":macro",
        ":synchronize",
        "//third_party:vulkan",
    ],
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
        ":macro",
        "//third_party:vulkan",
        "@lib-absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "vulkan",
    deps = [
        ":command",
    ],
    visibility = ["//jessie_steamer:__subpackages__"],
)

config_setting(
    name = "optimal_build",
    values = {"compilation_mode": "opt"},
)