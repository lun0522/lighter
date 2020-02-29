workspace(name = "jessie_steamer")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

git_repository(
    name = "lib-absl",
    remote = "https://github.com/abseil/abseil-cpp.git",
    commit = "b832dce8489ef7b6231384909fd9b68d5a5ff2b7",
    shallow_since = "1582671418 -0500",
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
    url = "https://sdk.lunarg.com/sdk/download/1.2.131.2/mac/vulkansdk-macos-1.2.131.2.tar.gz",
    sha256 = "e28363ae0bdb3d881ebf93cdd7a721d052f6a2e5686d0fb3447e6edd585bb53f",
    strip_prefix = "vulkansdk-macos-1.2.131.2/macOS",
    build_file = "//:third_party/BUILD.vulkan",
)

git_repository(
    name = "resource",
    remote = "https://github.com/lun0522/resource.git",
    commit = "fc80c718eb0ae3f0f2fb1043ce14102eb14b3289",
    shallow_since = "1582958847 -0800",
)