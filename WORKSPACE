workspace(name = "learn_vulkan")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "lib-glm",
    strip_prefix = "glm-0.9.9.4/glm",
    url = "https://github.com/g-truc/glm/archive/0.9.9.4.tar.gz",
    sha256 = "3a073eb8f3be07cee74481db0e1f78eda553b554941e405c863ab64de6a2e954",
    build_file = "//:third_party/BUILD.glm",
)

bind(
    name = "glm",
    actual = "@lib-glm//:glm",
)

new_git_repository(
    name = "lib-stb",
    remote = "https://github.com/nothings/stb.git",
    commit = "2c2908f50515dcd939f24be261c3ccbcd277bb49",
    shallow_since = "1551740933 -0800",
    build_file = "//:third_party/BUILD.stb",
)

bind(
    name = "stb",
    actual = "@lib-stb//:stb",
)