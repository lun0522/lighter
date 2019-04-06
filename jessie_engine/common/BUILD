package(default_visibility = ["//jessie_engine:__subpackages__"])

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
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    deps = ["//third_party:glm"],
)

cc_library(
    name = "window",
    srcs = ["window.cc"],
    hdrs = ["window.h"],
    deps = [
        ":util",
        "//third_party:glm",
        "//third_party:glfw",
        "//third_party/vulkan",
    ],
)