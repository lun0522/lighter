package(default_visibility = ["//lighter/application:__subpackages__"])

cc_library(
    name = "resource",
    data = [
        "//lighter/shader",
        "@resource//:resource",
    ],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    srcs = ["util.cc"],
    deps = ["//third_party/absl/flags:flag"],
)