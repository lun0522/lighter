cc_binary(
    name = "cube",
    srcs = ["cube.cc"],
    deps = [
        "//jessie_engine/common:util",
        "//jessie_engine/wrapper/vulkan:vulkan_wrapper",
        "//third_party/glm",
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
        "//jessie_engine/common:camera",
        "//jessie_engine/common:util",
        "//jessie_engine/common:window",
        "//jessie_engine/wrapper/vulkan:vulkan_wrapper",
        "//third_party/glm",
        "//third_party/vulkan",
    ],
    data = [
        "//jessie_engine/resource",
        "//jessie_engine/shader",
    ],
)