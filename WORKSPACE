workspace(name = "lighter")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@//:git_archives.bzl", "assimp_archive")
load("@//:git_archives.bzl", "freetype_archive")
load("@//:git_archives.bzl", "glfw_archive")

http_archive(
    name = "lib-absl",
    build_file = "//:third_party/absl/BUILD",
    sha256 = "306639352ec60dcbfc695405e989e1f63d0e55001582a5185b0a8caf2e8ea9ca",
    strip_prefix = "abseil-cpp-20200923.2",
    urls = ["https://github.com/abseil/abseil-cpp/archive/20200923.2.zip"],
)

http_archive(
    name = "lib-absl-headers",
    build_file = "//:third_party/absl/BUILD.headers",
    sha256 = "a68f7a8457b83e4cf0694c79e9bc4a838ac94d0a274737ebd3881c7732578464",
    strip_prefix = "lib-absl-20200923.2",
    urls = ["https://github.com/lun0522/lib-absl/archive/20200923.2.zip"],
)

assimp_archive(
    name = "lib-assimp-osx",
    build_file = "//:third_party/assimp/BUILD.osx",
    strip_prefix = "lib",
)

assimp_archive(
    name = "lib-assimp-ubuntu",
    build_file = "//:third_party/assimp/BUILD.ubuntu",
    strip_prefix = "lib",
)

assimp_archive(
    name = "lib-assimp-headers",
    build_file = "//:third_party/assimp/BUILD.headers",
    strip_prefix = "include",
)

freetype_archive(
    name = "lib-freetype-osx",
    build_file = "//:third_party/freetype/BUILD.osx",
    strip_prefix = "lib/osx",
)

freetype_archive(
    name = "lib-freetype-ubuntu",
    build_file = "//:third_party/freetype/BUILD.ubuntu",
    strip_prefix = "lib/ubuntu",
)

freetype_archive(
    name = "lib-freetype-headers",
    build_file = "//:third_party/freetype/BUILD.headers",
    strip_prefix = "include",
)

http_archive(
    name = "lib-glad",
    build_file = "//:third_party/BUILD.glad",
    sha256 = "fb70142b175aff3fc9cc0438ea4b2047c6d37163a7afe6942a6868755ec3e636",
    strip_prefix = "lib-glad-4.5",
    urls = ["https://github.com/lun0522/lib-glad/archive/4.5.zip"],
)

glfw_archive(
    name = "lib-glfw-headers",
    build_file = "//:third_party/glfw/BUILD.headers",
    strip_prefix = "include",
)

glfw_archive(
    name = "lib-glfw-osx",
    build_file = "//:third_party/glfw/BUILD.osx",
    strip_prefix = "lib",
)

glfw_archive(
    name = "lib-glfw-osx",
    build_file = "//:third_party/glfw/BUILD.osx",
    strip_prefix = "lib/osx",
)

glfw_archive(
    name = "lib-glfw-ubuntu",
    build_file = "//:third_party/glfw/BUILD.ubuntu",
    strip_prefix = "lib/ubuntu",
)

http_archive(
    name = "lib-glm",
    build_file = "//:third_party/BUILD.glm",
    sha256 = "4605259c22feadf35388c027f07b345ad3aa3b12631a5a316347f7566c6f1839",
    strip_prefix = "glm-0.9.9.8/glm",
    urls = ["https://github.com/g-truc/glm/archive/0.9.9.8.zip"],
)

http_archive(
    name = "lib-googletest",
    sha256 = "94c634d499558a76fa649edb13721dce6e98fb1e7018dfaeba3cd7a083945e91",
    strip_prefix = "googletest-release-1.10.0",
    urls = ["https://github.com/google/googletest/archive/release-1.10.0.zip"],
)

new_git_repository(
    name = "lib-stb",
    build_file = "//:third_party/BUILD.stb",
    commit = "b42009b3b9d4ca35bc703f5310eedc74f584be58",
    remote = "https://github.com/nothings/stb.git",
    shallow_since = "1594640766 -0700",
)

http_archive(
    name = "lib-vulkan-linux",
    build_file = "//:third_party/vulkan/BUILD.linux",
    sha256 = "6d8828fa9c9113ef4083a07994cf0eb13b8d239a5263bd95aa408d2f57585268",
    strip_prefix = "1.2.154.0/x86_64",
    url = "https://sdk.lunarg.com/sdk/download/1.2.154.0/linux/vulkansdk-linux-x86_64-1.2.154.0.tar.gz",
)

http_archive(
    name = "lib-vulkan-osx",
    build_file = "//:third_party/vulkan/BUILD.osx",
    sha256 = "81da27908836f6f5f41ed7962ff1b4be56ded3b447d4802a98b253d492f985cf",
    strip_prefix = "vulkansdk-macos-1.2.135.0/macOS",
    url = "https://sdk.lunarg.com/sdk/download/1.2.135.0/mac/vulkansdk-macos-1.2.135.0.tar.gz",
)

http_archive(
    name = "resource",
    sha256 = "f1f3ef3d9cda4e9a74171719c3e26aef9b6eeae0b16a9a4f6f3addc1ba2bc131",
    strip_prefix = "resource-1.0.0",
    urls = ["https://github.com/lun0522/resource/archive/1.0.0.zip"],
)
