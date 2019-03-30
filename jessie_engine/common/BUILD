cc_library(
    name = "camera",
    srcs = ["camera.cc"],
    hdrs = ["camera.h"],
    deps = [
        ":window",
        "//third_party/glm:glm",
    ],
)

cc_library(
    name = "util",
    srcs = ["util.cc"],
    hdrs = ["util.h"],
    deps = ["//third_party/glm:glm"],
)

cc_library(
    name = "window",
    srcs = ["window.cc"],
    hdrs = ["window.h"],
    deps = [
        ":util",
        "//third_party/glfw:glfw",
        "//third_party/glm:glm",
        "//third_party/vulkan:vulkan",
    ],
)