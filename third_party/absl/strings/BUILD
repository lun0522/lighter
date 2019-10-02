licenses(["notice"])

load("//third_party/absl:generate_dependency.bzl", "absl_library")

absl_library(
    name = "strings",
    hdrs = [
        "@lib-absl//absl/strings:str_cat.h",
        "@lib-absl//absl/strings:str_join.h",
        "@lib-absl//absl/strings:str_split.h",
        "@lib-absl//absl/strings:string_view.h",
    ],
    deps = ["@lib-absl//absl/strings"],
)

absl_library(
    name = "str_format",
    hdrs = ["@lib-absl//absl/strings:str_format.h"],
    deps = ["@lib-absl//absl/strings:str_format"],
)