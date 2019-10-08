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

new_git_repository(
    name = "lib-assimp",
    remote = "https://github.com/assimp/assimp.git",
    commit = "80799bdbf90ce626475635815ee18537718a05b1",
    shallow_since = "1512998565 +0100",
    strip_prefix = "include",
    build_file = "//:third_party/BUILD.assimp",
)

http_archive(
    name = "lib-freetype",
    url = "https://download.savannah.gnu.org/releases/freetype/freetype-2.10.0.tar.gz",
    sha256 = "955e17244e9b38adb0c98df66abb50467312e6bb70eac07e49ce6bd1a20e809a",
    strip_prefix = "freetype-2.10.0/include",
    build_file = "//:third_party/BUILD.freetype",
)

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
    commit = "2c2908f50515dcd939f24be261c3ccbcd277bb49",
    shallow_since = "1551740933 -0800",
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
    commit = "b18a663156e6e86b33d62dbe11e094b2ae3a3118",
    shallow_since = "1559518487 -0700",
)