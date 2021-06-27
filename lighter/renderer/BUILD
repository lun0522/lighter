load("//:settings.bzl", "graphics_api")

graphics_api()

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    visibility = ["//lighter:__subpackages__"],
    deps = [
        "//lighter/common:graphics_api",
        "//lighter/common:util",
        "//lighter/common:window",
        "//lighter/renderer/ir:renderer",
        "//lighter/renderer/ir:type",
        "//third_party:absl",
    ] + select({
        "use_vulkan": ["//lighter/renderer/vk:renderer"],
        "//conditions:default": [],
    }),
)
