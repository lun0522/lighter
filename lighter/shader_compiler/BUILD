package(default_visibility = ["//visibility:private"])

cc_library(
    name = "compilation_record",
    srcs = ["compilation_record.cc"],
    hdrs = ["compilation_record.h"],
    deps = [
        ":util",
        "//lighter/common:graphics_api",
        "//lighter/common:util",
        "//third_party:absl",
    ],
)

cc_library(
    name = "compiler",
    srcs = ["compiler.cc"],
    hdrs = ["compiler.h"],
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
    srcs = ["run_compiler.cc"],
    hdrs = ["run_compiler.h"],
    deps = [
        ":compilation_record",
        ":compiler",
        ":util",
        "//lighter/common:data",
        "//lighter/common:file",
        "//lighter/common:graphics_api",
        "//lighter/common:timer",
        "//lighter/common:util",
        "//third_party:absl",
        "//third_party:picosha2",
    ],
)

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    visibility = ["//visibility:public"],
    deps = ["//lighter/common:graphics_api"],
)
