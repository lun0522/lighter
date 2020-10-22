package(default_visibility = ["//visibility:private"])

cc_binary(
    name = "traingle",
    srcs = ["triangle.cc"],
    deps = [
        ":util",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    deps = [
        "//lighter/common:file",
        "//lighter/common:util",
        "//lighter/renderer:onscreen_renderer",
        "//lighter/renderer/vk:renderer",
        "//third_party:absl",
    ],
)
