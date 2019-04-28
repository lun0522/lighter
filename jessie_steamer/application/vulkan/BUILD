package(default_visibility = ["//visibility:private"])

cc_binary(
    name = "cube",
    srcs = ["cube.cc"],
    deps = [
        "//jessie_steamer/common:util",
        "//jessie_steamer/common:window",
        "//jessie_steamer/wrapper/vulkan:vulkan_wrapper",
        "//third_party:glm",
        "//third_party:vulkan",
        "@lib-absl//absl/types:optional",
    ],
    data = [
        "//jessie_steamer/resource",
        "//jessie_steamer/shader",
    ],
)

cc_binary(
    name = "nanosuit",
    srcs = ["nanosuit.cc"],
    deps = [
        "//jessie_steamer/common:camera",
        "//jessie_steamer/common:util",
        "//jessie_steamer/common:window",
        "//jessie_steamer/wrapper/vulkan:vulkan_wrapper",
        "//third_party:glm",
        "//third_party:vulkan",
        "@lib-absl//absl/types:optional",
    ],
    data = [
        "//jessie_steamer/resource",
        "//jessie_steamer/shader",
    ],
)

cc_binary(
    name = "planet",
    srcs = ["planet.cc"],
    deps = [
        "//jessie_steamer/common:camera",
        "//jessie_steamer/common:util",
        "//jessie_steamer/common:window",
        "//jessie_steamer/wrapper/vulkan:vulkan_wrapper",
        "//third_party:glm",
        "//third_party:vulkan",
        "@lib-absl//absl/types:optional",
    ],
    data = [
        "//jessie_steamer/resource",
        "//jessie_steamer/shader",
    ],
)