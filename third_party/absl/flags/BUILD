licenses(["notice"])

load("//third_party/absl:generate_dependency.bzl", "absl_library")

absl_library(
    name = "flag",
    hdrs = [
        "@lib-absl//absl/flags:declare.h",
        "@lib-absl//absl/flags:flag.h",
    ],
    deps = ["@lib-absl//absl/flags:flag"],
)

absl_library(
    name = "parse",
    hdrs = ["@lib-absl//absl/flags:parse.h"],
    deps = ["@lib-absl//absl/flags:parse"],
)
