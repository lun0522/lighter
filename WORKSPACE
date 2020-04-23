workspace(name = "jessie_steamer")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

git_repository(
    name = "lib-absl",
    remote = "https://github.com/abseil/abseil-cpp.git",
    commit = "c51510d1d87ebce8615ae1752fd5aca912f6cf4c",
    shallow_since = "1587584588 -0400",
)

# NOTE: If this changes, remember to update the dynamic library.
new_git_repository(
    name = "lib-assimp",
    remote = "https://github.com/assimp/assimp.git",
    commit = "8f0c6b04b2257a520aaab38421b2e090204b69df",
    shallow_since = "1578830200 +0100",
    strip_prefix = "include",
    build_file = "//:third_party/BUILD.assimp",
)

# NOTE: If this changes, remember to update the dynamic library.
http_archive(
    name = "lib-freetype",
    url = "https://download.savannah.gnu.org/releases/freetype/freetype-2.10.1.tar.gz",
    sha256 = "3a60d391fd579440561bf0e7f31af2222bc610ad6ce4d9d7bd2165bca8669110",
    strip_prefix = "freetype-2.10.1/include",
    build_file = "//:third_party/BUILD.freetype",
)

# NOTE: If this changes, remember to update the dynamic library.
new_git_repository(
    name = "lib-glfw",
    remote = "https://github.com/glfw/glfw.git",
    commit = "0a49ef0a00baa3ab520ddc452f0e3b1e099c5589",
    shallow_since = "1579473811 +0100",
    strip_prefix = "include/GLFW",
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
    commit = "5436e1e4f1462f1b4184614329b0ce507c6a7ba2",
    shallow_since = "1587606630 -0700",
)