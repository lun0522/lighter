package(default_visibility = ["//lighter:__subpackages__"])

load("//:settings.bzl", "compilation_mode")

compilation_mode()

cc_library(
    name = "offscreen",
    deps = [
        ":align",
        ":compute_pass",
        ":graphics_pass",
        ":model",
        ":offscreen_wrappers",
        ":text",
    ],
    data = ["@resource//:resource"],
)

cc_library(
    name = "onscreen",
    deps = [
        ":offscreen",
        "//lighter/renderer/vulkan/wrapper:window",
    ],
)

cc_library(
    name = "align",
    hdrs = ["align.h"],
)

cc_library(
    name = "base_pass",
    hdrs = ["base_pass.h"],
    srcs = ["base_pass.cc"],
    deps = [
        "//lighter/common:util",
        "//lighter/renderer/vulkan/wrapper:image",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/types:optional",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "compute_pass",
    hdrs = ["compute_pass.h"],
    srcs = ["compute_pass.cc"],
    deps = [
        ":base_pass",
        "//lighter/common:util",
        "//lighter/renderer/vulkan/wrapper:image",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:span",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "graphics_pass",
    hdrs = ["graphics_pass.h"],
    srcs = ["graphics_pass.cc"],
    deps = [
        ":base_pass",
    ],
)

cc_library(
    name = "model",
    hdrs = ["model.h"],
    srcs = ["model.cc"],
    deps = [
        ":offscreen_wrappers",
        "//lighter/common:file",
        "//lighter/common:model_loader",
        "//lighter/common:util",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/memory",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:variant",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "offscreen_wrappers",
    deps = [
        "//lighter/renderer/vulkan/wrapper:basics",
        "//lighter/renderer/vulkan/wrapper:buffer",
        "//lighter/renderer/vulkan/wrapper:command",
        "//lighter/renderer/vulkan/wrapper:descriptor",
        "//lighter/renderer/vulkan/wrapper:image",
        "//lighter/renderer/vulkan/wrapper:pipeline",
        "//lighter/renderer/vulkan/wrapper:render_pass",
        "//lighter/renderer/vulkan/wrapper:synchronization",
        "//lighter/renderer/vulkan/wrapper:util",
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
        ":offscreen_wrappers",
        "//lighter/common:char_lib",
        "//lighter/common:file",
        "//lighter/common:util",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/memory",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party/absl/types:span",
        "//third_party:glm",
    ],
)