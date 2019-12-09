workspace(name = "jessie_steamer")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

git_repository(
    name = "lib-absl",
    remote = "https://github.com/abseil/abseil-cpp.git",
    commit = "aa844899c937bde5d2b24f276b59997e5b668bde",
    shallow_since = "1565288385 -0400",
)

# NOTE: If this changes, remember to update the dynamic library.
new_git_repository(
    name = "lib-assimp",
    remote = "https://github.com/assimp/assimp.git",
    commit = "80799bdbf90ce626475635815ee18537718a05b1",
    shallow_since = "1512998565 +0100",
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
    commit = "b0796109629931b6fa6e449c15a177845256a407",
    shallow_since = "1555371630 +0200",
    strip_prefix = "include/GLFW",
    build_file = "//:third_party/BUILD.glfw",
)

new_git_repository(
    name = "lib-glm",
    remote = "https://github.com/g-truc/glm.git",
    commit = "4db8f89aace8f04c839b606e15b39fb8383ec732",
    shallow_since = "1567951122 +0200",
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
    url = "https://sdk.lunarg.com/sdk/download/1.1.121.1/mac/vulkansdk-macos-1.1.121.1.tar.gz",
    sha256 = "c4a177a2a08bb2496dff02cdc1730ce5893c599ae9b6b9867895970a5f987c4f",
    strip_prefix = "vulkansdk-macos-1.1.121.1/macOS",
    build_file = "//:third_party/BUILD.vulkan",
)

git_repository(
    name = "resource",
    remote = "https://github.com/lun0522/resource.git",
    commit = "ce8e139cbc681466974a3aa516d1d2f737049546",
    shallow_since = "1575869992 -0800",
)