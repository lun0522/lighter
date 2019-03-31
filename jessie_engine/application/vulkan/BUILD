package(default_visibility = ["//visibility:private"])

cc_binary(
    name = "cube",
    srcs = ["cube.cc"],
    deps = [
        "//external:glm",
        "//jessie_engine/common:util",
        "//jessie_engine/wrapper/vulkan:vulkan_wrapper",
        "//third_party/vulkan",
    ],
    data = [
        "//jessie_engine/resource",
        "//jessie_engine/shader",
    ],
)

cc_binary(
    name = "nanosuit",
    srcs = ["nanosuit.cc"],
    deps = [
        "//external:glm",
        "//jessie_engine/common:camera",
        "//jessie_engine/common:util",
        "//jessie_engine/common:window",
        "//jessie_engine/wrapper/vulkan:vulkan_wrapper",
        "//third_party/vulkan",
    ],
    data = [
        "//jessie_engine/resource",
        "//jessie_engine/shader",
    ],
)