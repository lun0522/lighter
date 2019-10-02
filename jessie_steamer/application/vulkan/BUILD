package(default_visibility = ["//visibility:private"])

cc_library(
    name = "common",
    hdrs = ["util.h"],
    srcs = ["util.cc"],
    deps = [
        "//jessie_steamer/application:resource",
        "//jessie_steamer/common:file",
        "//jessie_steamer/common:util",
        "//jessie_steamer/wrapper/vulkan:on_screen",
        "//third_party/absl/flags:flag",
        "//third_party:vulkan",
    ],
)

cc_binary(
    name = "cube",
    srcs = ["cube.cc"],
    deps = [
        ":common",
        "//jessie_steamer/common:time",
        "//third_party/absl/memory",
        "//third_party:glm",
    ],
)

cc_binary(
    name = "nanosuit",
    srcs = ["nanosuit.cc"],
    deps = [
        ":common",
        "//jessie_steamer/common:camera",
        "//jessie_steamer/common:time",
        "//third_party/absl/memory",
        "//third_party:glm",
    ],
)

cc_binary(
    name = "planet",
    srcs = ["planet.cc"],
    deps = [
        ":common",
        "//jessie_steamer/common:camera",
        "//jessie_steamer/common:time",
        "//third_party/absl/memory",
        "//third_party:glm",
    ],
)