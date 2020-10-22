package(default_visibility = ["//lighter:__subpackages__"])

load("//:settings.bzl", "compilation_mode")

compilation_mode()

cc_library(
    name = "program",
    hdrs = ["program.h"],
    srcs = ["program.cc"],
    deps = [
        "//lighter/common:file",
        "//lighter/common:ref_count",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:glad",
    ],
)