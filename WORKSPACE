workspace(name = "jessie_steamer")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")

git_repository(
    name = "lib-absl",
    remote = "https://github.com/abseil/abseil-cpp.git",
    commit = "fcb104594b0bb4b8ac306cb2f55ecdad40974683",
    shallow_since = "1543960480 -0500",
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
    commit = "d162eee1e6f7c317a09229fe6ceab8ec6ab9a4b4",
    shallow_since = "1554197305 +0200",
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
    url = "https://sdk.lunarg.com/sdk/download/1.1.92.1/mac/vulkansdk-macos-1.1.92.1.tar.gz",
    sha256 = "1dc5c758ba83cc0b1e3baa533a5b2052afa378df87a84ee3e56ab6d97df12865",
    strip_prefix = "vulkansdk-macos-1.1.92.1/macOS",
    build_file = "//:third_party/BUILD.vulkan",
)

git_repository(
    name = "resource",
    remote = "https://github.com/lun0522/resource.git",
    commit = "e6b3e8eab610a502f3a1ce81da1e8e9d9b85072a",
    shallow_since = "1558913515 -0700",
)