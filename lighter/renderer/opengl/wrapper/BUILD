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
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/types:optional",
        "//third_party:glad",
    ],
)