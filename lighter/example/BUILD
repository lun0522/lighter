package(default_visibility = ["//visibility:private"])

cc_binary(
    name = "triangle",
    srcs = ["triangle.cc"],
    deps = [":util"],
)

cc_library(
    name = "util",
    hdrs = ["util.h"],
    data = [
        "//lighter/shader",
        "@resource",
    ],
    deps = [
        "//lighter/common:data",
        "//lighter/common:file",
        "//lighter/common:graphics_api",
        "//lighter/common:timer",
        "//lighter/common:util",
        "//lighter/common:window",
        "//lighter/renderer:util",
        "//lighter/renderer/vk:renderer",
        "//third_party:absl",
        "//third_party:glm",
        "//third_party:spirv_cross",
    ],
)
