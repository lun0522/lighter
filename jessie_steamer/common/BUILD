package(default_visibility = ["//jessie_steamer:__subpackages__"])

cc_library(
    name = "camera",
    srcs = ["camera.cc"],
    hdrs = ["camera.h"],
    deps = [
        ":window",
        "//third_party:glm",
    ],
)

cc_library(
    name = "model_loader",
    srcs = ["model_loader.cc"],
    hdrs = ["model_loader.h"],
    deps = [
        ":util",
        "//third_party:assimp",
        "@lib-absl//absl/memory",
    ],
)

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    deps = [
        "//third_party:freetype",
        "//third_party:glm",
        "//third_party:stb",
        "@lib-absl//absl/container:flat_hash_map",
        "@lib-absl//absl/container:flat_hash_set",
        "@lib-absl//absl/memory",
        "@lib-absl//absl/types:optional",
    ],
)

cc_library(
    name = "window",
    srcs = ["window.cc"],
    hdrs = ["window.h"],
    deps = [
        ":util",
        "//third_party:glm",
        "//third_party:glfw",
        "//third_party:vulkan",
    ],
)