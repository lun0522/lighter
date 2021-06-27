package(default_visibility = ["//visibility:private"])

cc_library(
    name = "common",
    hdrs = ["util.h"],
    visibility = ["//lighter/application/opengl:__subpackages__"],
    deps = [
        "//lighter/application:resource",
        "//lighter/common:file",
        "//lighter/common:graphics_api",
        "//lighter/common:timer",
        "//lighter/common:util",
        "//lighter/common:window",
        "//lighter/renderer:util",
        "//lighter/renderer/opengl/wrapper:program",
        "//third_party:absl",
        "//third_party:glad",
        "//third_party:glfw",
        "//third_party:glm",
    ],
)

cc_binary(
    name = "triangle",
    srcs = ["triangle.cc"],
    deps = [":common"],
)
