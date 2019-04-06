workspace(name = "learn_vulkan")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")

# TODO: replace with stable release
http_file(
    name = "lib-glfw",
    urls = ["https://raw.githubusercontent.com/glfw/glfw/master/include/GLFW/glfw3.h"],
    downloaded_file_path = "glfw3.h",
)

new_git_repository(
    name = "lib-glm",
    strip_prefix = "glm",
    remote = "https://github.com/g-truc/glm.git",
    commit = "d162eee1e6f7c317a09229fe6ceab8ec6ab9a4b4",
    shallow_since = "1554197305 +0200",
    build_file = "//:third_party/BUILD.glm",
)

new_git_repository(
    name = "lib-stb",
    remote = "https://github.com/nothings/stb.git",
    commit = "2c2908f50515dcd939f24be261c3ccbcd277bb49",
    shallow_since = "1551740933 -0800",
    build_file = "//:third_party/BUILD.stb",
)