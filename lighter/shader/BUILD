package(default_visibility = ["//lighter:__subpackages__"])

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
        ":util",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:shaderc",
    ],
)

cc_binary(
    name = "compile_shaders",
    srcs = ["compile_shaders.cc"],
    deps = [
        ":run_compiler",
        ":util",
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
        ":util",
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
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    srcs = ["util.cc"],
    deps = ["//lighter/common:graphics_api"],
)
