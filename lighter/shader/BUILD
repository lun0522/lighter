package(default_visibility = ["//visibility:private"])

cc_library(
    name = "compilation_record",
    hdrs = ["compilation_record.h"],
    srcs = ["compilation_record.cc"],
    deps = [
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
        "//lighter/common:graphics_api",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:shaderc",
    ],
)

cc_binary(
    name = "compile_shaders",
    srcs = ["compile_shaders.cc"],
    deps = [
        ":compiler",
        ":run_compiler",
        "//lighter/common:util",
        "//third_party:absl",
    ],
)

cc_library(
    name = "run_compiler",
    hdrs = ["run_compiler.h"],
    srcs = ["run_compiler.cc"],
    deps = [
        ":compilation_record",
        ":compiler",
        "//lighter/common:file",
        "//lighter/common:graphics_api",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:picosha2",
    ],
)

filegroup(
    name = "shader",
    srcs = glob(["**/*.spv"]),
    visibility = ["//lighter:__subpackages__"],
)
