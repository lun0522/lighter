package(default_visibility = ["//visibility:private"])

cc_binary(
    name = "cube",
    srcs = ["cube.cc"],
    deps = [
        "//jessie_steamer/application:resource",
        "//jessie_steamer/common:time",
        "//jessie_steamer/common:window",
        "//jessie_steamer/wrapper/vulkan:vulkan_wrapper",
        "//third_party:glm",
        "//third_party:vulkan",
        "@lib-absl//absl/types:optional",
    ],
    copts = ["-DUSE_VULKAN"],
)

cc_binary(
    name = "nanosuit",
    srcs = ["nanosuit.cc"],
    deps = [
        "//jessie_steamer/application:resource",
        "//jessie_steamer/common:camera",
        "//jessie_steamer/common:time",
        "//jessie_steamer/common:window",
        "//jessie_steamer/wrapper/vulkan:vulkan_wrapper",
        "//third_party:glm",
        "//third_party:vulkan",
        "@lib-absl//absl/strings:str_format",
        "@lib-absl//absl/types:optional",
    ],
    copts = ["-DUSE_VULKAN"],
)

cc_binary(
    name = "planet",
    srcs = ["planet.cc"],
    deps = [
        "//jessie_steamer/application:resource",
        "//jessie_steamer/common:camera",
        "//jessie_steamer/common:time",
        "//jessie_steamer/common:window",
        "//jessie_steamer/wrapper/vulkan:vulkan_wrapper",
        "//third_party:glm",
        "//third_party:vulkan",
        "@lib-absl//absl/strings:str_format",
        "@lib-absl//absl/types:optional",
    ],
    copts = ["-DUSE_VULKAN"],
)