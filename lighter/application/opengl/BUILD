package(default_visibility = ["//visibility:private"])

cc_library(
    name = "common",
    hdrs = ["util.h"],
    deps = [
        "//lighter/common:file",
        "//lighter/common:timer",
        "//lighter/common:util",
        "//lighter/common:window",
        "//lighter/application:resource",
        "//lighter/application:util",
        "//lighter/renderer/common:align",
        "//lighter/renderer/opengl/wrapper:program",
        "//third_party/absl/container:flat_hash_map",
        "//third_party/absl/flags:flag",
        "//third_party/absl/memory",
        "//third_party:glad",
        "//third_party:glfw",
        "//third_party:glm",
    ],
    visibility = ["//lighter/application/opengl:__subpackages__"],
)

cc_binary(
    name = "triangle",
    srcs = ["triangle.cc"],
    deps = [":common"],
)