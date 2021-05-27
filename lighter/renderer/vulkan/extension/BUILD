package(default_visibility = ["//lighter:__subpackages__"])

load("//:settings.bzl", "compilation_mode")

compilation_mode()

cc_library(
    name = "offscreen",
    deps = [
        ":compute_pass",
        ":graphics_pass",
        ":model",
        ":naive_render_pass",
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
    name = "base_pass",
    hdrs = ["base_pass.h"],
    srcs = ["base_pass.cc"],
    deps = [
        "//lighter/common:util",
        "//lighter/renderer:image_usage",
        "//lighter/renderer/vulkan/wrapper:image",
        "//third_party:absl",
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
        "//lighter/renderer:image_usage",
        "//lighter/renderer/vulkan/wrapper:image",
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "graphics_pass",
    hdrs = ["graphics_pass.h"],
    srcs = ["graphics_pass.cc"],
    deps = [
        ":base_pass",
        "//lighter/common:util",
        "//lighter/renderer:image_usage",
        "//lighter/renderer/vulkan/wrapper:image",
        "//lighter/renderer/vulkan/wrapper:render_pass",
        "//third_party:absl",
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
        "//third_party:absl",
        "//third_party:vulkan",
    ],
)

cc_library(
    name = "naive_render_pass",
    hdrs = ["naive_render_pass.h"],
    srcs = ["naive_render_pass.cc"],
    deps = [
        ":graphics_pass",
        "//lighter/common:util",
        "//lighter/renderer/vulkan/wrapper:basics",
        "//lighter/renderer/vulkan/wrapper:render_pass",
        "//third_party:absl",
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
        ":naive_render_pass",
        ":offscreen_wrappers",
        "//lighter/common:char_lib",
        "//lighter/common:file",
        "//lighter/common:graphics_api",
        "//lighter/common:image",
        "//lighter/common:util",
        "//lighter/renderer:align",
        "//third_party:absl",
        "//third_party:glm",
    ],
)