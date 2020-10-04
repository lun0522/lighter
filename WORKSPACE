workspace(name = "lighter")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@//:git_archives.bzl", "assimp_archive")
load("@//:git_archives.bzl", "freetype_archive")

http_archive(
    name = "lib-absl",
    urls = ["https://github.com/abseil/abseil-cpp/archive/20200225.2.zip"],
    strip_prefix = "abseil-cpp-20200225.2",
    sha256 = "f342aac71a62861ac784cadb8127d5a42c6c61ab1cd07f00aef05f2cc4988c42",
)

assimp_archive(
    name = "lib-assimp",
    strip_prefix = "lib",
    build_file = "//:third_party/BUILD.assimp",
)

assimp_archive(
    name = "lib-assimp-headers",
    strip_prefix = "include",
    build_file = "//:third_party/BUILD.assimp_headers",
)

freetype_archive(
    name = "lib-freetype",
    strip_prefix = "lib/osx",
    build_file = "//:third_party/BUILD.freetype",
)

freetype_archive(
    name = "lib-freetype-headers",
    strip_prefix = "include",
    build_file = "//:third_party/BUILD.freetype_headers",
)

http_archive(
    name = "lib-glad",
    urls = ["https://github.com/lun0522/lib-glad/archive/4.1.zip"],
    strip_prefix = "lib-glad-4.1",
    sha256 = "9909c0fb6232e215a78414bb6c72c6de49adbc09c6607a93b9cd2d9a5ccd6774",
    build_file = "//:third_party/BUILD.glad",
)

http_archive(
    name = "lib-glfw",
    urls = ["https://github.com/lun0522/lib-glfw/archive/3.3.2.zip"],
    strip_prefix = "lib-glfw-3.3.2",
    sha256 = "db9fd85b17e0b3545fe092451076963795636f00bde33c892f888ab17224849f",
    build_file = "//:third_party/BUILD.glfw",
)

http_archive(
    name = "lib-glm",
    urls = ["https://github.com/g-truc/glm/archive/0.9.9.8.zip"],
    strip_prefix = "glm-0.9.9.8/glm",
    sha256 = "4605259c22feadf35388c027f07b345ad3aa3b12631a5a316347f7566c6f1839",
    build_file = "//:third_party/BUILD.glm",
)

http_archive(
  name = "lib-googletest",
  urls = ["https://github.com/google/googletest/archive/release-1.10.0.zip"],
  strip_prefix = "googletest-release-1.10.0",
  sha256 = "94c634d499558a76fa649edb13721dce6e98fb1e7018dfaeba3cd7a083945e91",
)

new_git_repository(
    name = "lib-stb",
    remote = "https://github.com/nothings/stb.git",
    commit = "b42009b3b9d4ca35bc703f5310eedc74f584be58",
    shallow_since = "1594640766 -0700",
    build_file = "//:third_party/BUILD.stb",
)

http_archive(
    name = "lib-vulkan",
    url = "https://sdk.lunarg.com/sdk/download/1.2.135.0/mac/vulkansdk-macos-1.2.135.0.tar.gz",
    sha256 = "81da27908836f6f5f41ed7962ff1b4be56ded3b447d4802a98b253d492f985cf",
    strip_prefix = "vulkansdk-macos-1.2.135.0/macOS",
    build_file = "//:third_party/BUILD.vulkan",
)

http_archive(
  name = "resource",
  urls = ["https://github.com/lun0522/resource/archive/1.0.0.zip"],
  strip_prefix = "resource-1.0.0",
  sha256 = "f1f3ef3d9cda4e9a74171719c3e26aef9b6eeae0b16a9a4f6f3addc1ba2bc131",
)