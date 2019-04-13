workspace(name = "learn_vulkan")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")

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

http_archive(
    name = "lib-vulkan",
    url = "https://sdk.lunarg.com/sdk/download/1.1.92.1/mac/vulkansdk-macos-1.1.92.1.tar.gz",
    sha256 = "1dc5c758ba83cc0b1e3baa533a5b2052afa378df87a84ee3e56ab6d97df12865",
    strip_prefix = "vulkansdk-macos-1.1.92.1/macOS",
    build_file = "//:third_party/BUILD.vulkan",
)