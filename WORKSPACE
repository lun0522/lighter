workspace(name = "lighter")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@//:git_repository.bzl", "assimp_repository")
load("@//:git_repository.bzl", "freetype_repository")

git_repository(
    name = "lib-absl",
    remote = "https://github.com/abseil/abseil-cpp.git",
    commit = "c51510d1d87ebce8615ae1752fd5aca912f6cf4c",
    shallow_since = "1587584588 -0400",
)

assimp_repository(
    name = "lib-assimp",
    build_file = "//:third_party/BUILD.assimp",
)

assimp_repository(
    name = "lib-assimp-headers",
    strip_prefix = "include",
    build_file = "//:third_party/BUILD.assimp_headers",
)

freetype_repository(
    name = "lib-freetype",
    build_file = "//:third_party/BUILD.freetype",
)

freetype_repository(
    name = "lib-freetype-headers",
    strip_prefix = "include",
    build_file = "//:third_party/BUILD.freetype_headers",
)

new_git_repository(
    name = "lib-glfw",
    remote = "https://github.com/lun0522/lib-glfw.git",
    commit = "e4ddcfb6db1053774ec97a740d1193d02a5195a2",
    shallow_since = "1587843888 -0700",
    build_file = "//:third_party/BUILD.glfw",
)

new_git_repository(
    name = "lib-glm",
    remote = "https://github.com/g-truc/glm.git",
    commit = "13724cfae64a8b5313d1cabc9a963d2c9dbeda12",
    shallow_since = "1578255577 +0100",
    strip_prefix = "glm",
    build_file = "//:third_party/BUILD.glm",
)

new_git_repository(
    name = "lib-stb",
    remote = "https://github.com/nothings/stb.git",
    commit = "052dce117ed989848a950308bd99eef55525dfb1",
    shallow_since = "1566060782 -0700",
    build_file = "//:third_party/BUILD.stb",
)

http_archive(
    name = "lib-vulkan",
    url = "https://sdk.lunarg.com/sdk/download/1.2.135.0/mac/vulkansdk-macos-1.2.135.0.tar.gz",
    sha256 = "81da27908836f6f5f41ed7962ff1b4be56ded3b447d4802a98b253d492f985cf",
    strip_prefix = "vulkansdk-macos-1.2.135.0/macOS",
    build_file = "//:third_party/BUILD.vulkan",
)

git_repository(
    name = "resource",
    remote = "https://github.com/lun0522/resource.git",
    commit = "876c45864672c9fdfcf3c69816f9bfe37ccfd6d9",
    shallow_since = "1587848201 -0700",
)