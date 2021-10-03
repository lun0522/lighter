load("//:settings.bzl", "compilation_mode")

package(default_visibility = ["//lighter:__subpackages__"])

compilation_mode()

cc_library(
    name = "program",
    srcs = ["program.cc"],
    hdrs = ["program.h"],
    deps = [
        "//lighter/common:data",
        "//lighter/common:file",
        "//lighter/common:ref_count",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:glad",
    ],
)
