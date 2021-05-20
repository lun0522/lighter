package(default_visibility = ["//visibility:private"])

cc_library(
    name = "compilation_record",
    hdrs = ["compilation_record.h"],
    srcs = ["compilation_record.cc"],
    deps = [
        ":util",
        "//lighter/common:graphics_api",
        "//lighter/common:util",
        "//third_party:absl",
    ],
)

cc_library(
    name = "compiler",
    hdrs = ["compiler.h"],
    srcs = ["compiler.cc"],
    deps = [
        ":compilation_record",
        "//lighter/common:graphics_api",
        "//lighter/common:util",
        "//third_party:absl",
    ],
)

cc_binary(
    name = "compile_shaders",
    srcs = ["compile_shaders.cc"],
    deps = [
        ":compiler",
        "//third_party:absl",
    ],
)

filegroup(
    name = "shader",
    srcs = glob(["**/*.spv"]),
    visibility = ["//lighter:__subpackages__"],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    srcs = ["util.cc"],
    deps = ["//lighter/common:graphics_api"],
)
