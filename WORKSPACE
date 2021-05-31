workspace(name = "lighter")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@//:local_archives.bzl", "use_vulkan_sdk")
load("@//:remote_archives.bzl", "assimp_archive", "freetype_archive",
     "glfw_archive", "spirv_cross_archive")

#######################################
# rules_foreign_cc

http_archive(
    name = "rules_foreign_cc",
    sha256 = "d54742ffbdc6924f222d2179f0e10e911c5c659c4ae74158e9fe827aad862ac6",
    strip_prefix = "rules_foreign_cc-0.2.0",
    url = "https://github.com/bazelbuild/rules_foreign_cc/archive/0.2.0.tar.gz",
)

load("@rules_foreign_cc//foreign_cc:repositories.bzl",
     "rules_foreign_cc_dependencies")

# Set up common toolchains for building targets. See more details:
# https://github.com/bazelbuild/rules_foreign_cc/tree/main/docs#rules_foreign_cc_dependencies
rules_foreign_cc_dependencies()

#######################################
# Abseil

http_archive(
    name = "lib-absl-include",
    build_file = "//:third_party/absl/BUILD.include",
    sha256 = "b4c76fbf553934eb03893b6262d48bca9ab06784331e83bce762e5e493b55f7b",
    strip_prefix = "lib-absl-20210324.1",
    urls = ["https://github.com/lun0522/lib-absl/archive/20210324.1.zip"],
)

http_archive(
    name = "lib-absl",
    build_file = "//:third_party/absl/BUILD",
    sha256 = "cfb1f22164808eb0a233ad91287df84c2af2084cfc8b429eca1be1e57511065d",
    strip_prefix = "abseil-cpp-20210324.1",
    urls = ["https://github.com/abseil/abseil-cpp/archive/20210324.1.zip"],
)

#######################################
# Assimp

assimp_archive(
    name = "lib-assimp",
    build_file = "//:third_party/BUILD.assimp",
)

assimp_archive(
    name = "lib-assimp-all-srcs",
    build_file = "//:third_party/BUILD.all_srcs",
)

#######################################
# FreeType

freetype_archive(
    name = "lib-freetype",
    build_file = "//:third_party/BUILD.freetype",
)

freetype_archive(
    name = "lib-freetype-all-srcs",
    build_file = "//:third_party/BUILD.all_srcs",
)

#######################################
# GLAD

http_archive(
    name = "lib-glad",
    build_file = "//:third_party/BUILD.glad",
    sha256 = "fb70142b175aff3fc9cc0438ea4b2047c6d37163a7afe6942a6868755ec3e636",
    strip_prefix = "lib-glad-4.5",
    urls = ["https://github.com/lun0522/lib-glad/archive/4.5.zip"],
)

#######################################
# GLFW

glfw_archive(
    name = "lib-glfw",
    build_file = "//:third_party/BUILD.glfw",
)

glfw_archive(
    name = "lib-glfw-all-srcs",
    build_file = "//:third_party/BUILD.all_srcs",
)

#######################################
# GLM

http_archive(
    name = "lib-glm",
    build_file = "//:third_party/BUILD.glm",
    sha256 = "4605259c22feadf35388c027f07b345ad3aa3b12631a5a316347f7566c6f1839",
    strip_prefix = "glm-0.9.9.8",
    urls = ["https://github.com/g-truc/glm/archive/0.9.9.8.zip"],
)

#######################################
# GoogleTest

http_archive(
    name = "lib-googletest",
    sha256 = "94c634d499558a76fa649edb13721dce6e98fb1e7018dfaeba3cd7a083945e91",
    strip_prefix = "googletest-release-1.10.0",
    urls = ["https://github.com/google/googletest/archive/release-1.10.0.zip"],
)

#######################################
# PicoSHA2

new_git_repository(
    name = "lib-picosha2",
    build_file = "//:third_party/BUILD.picosha2",
    commit = "b699e6c900be6e00152db5a3d123c1db42ea13d0",
    remote = "https://github.com/okdshin/PicoSHA2.git",
    shallow_since = "1531968639 +0900",
)

#######################################
# shaderc

use_vulkan_sdk(
    name = "lib-shaderc-osx",
    build_file_abs_path = __workspace_dir__ + "/third_party/shaderc/BUILD.osx",
)

use_vulkan_sdk(
    name = "lib-shaderc-windows",
    build_file_abs_path = __workspace_dir__ + "/third_party/shaderc/BUILD.windows",
)

#######################################
# SPIRV-Cross

spirv_cross_archive(
    platform = "linux",
    url = "https://github.com/KhronosGroup/SPIRV-Cross/releases/download/2021-01-15/spirv-cross-clang-trusty-64bit-9acb9ec31f.tar.gz",
    sha256 = "2e207445efab417ce737c9f320690f5e252d6ac7ee167a2911ef21c145f564f7",
)

spirv_cross_archive(
    platform = "osx",
    url = "https://github.com/KhronosGroup/SPIRV-Cross/releases/download/2021-01-15/spirv-cross-clang-macos-64bit-9acb9ec31f.tar.gz",
    sha256 = "6f43900361deb5e2d4df481d812bd83ebdf336dce3dbfa60d9ef3d44f2d09264",
)

spirv_cross_archive(
    platform = "windows",
    url = "https://github.com/KhronosGroup/SPIRV-Cross/releases/download/2021-01-15/spirv-cross-vs2017-64bit-9acb9ec31f.tar.gz",
    sha256 = "f27bd8aeb36991c9e893d516dc680503ad66b2c548ba540c8b551afa0d230c03",
)

#######################################
# stb

new_git_repository(
    name = "lib-stb",
    build_file = "//:third_party/BUILD.stb",
    commit = "c9064e317699d2e495f36ba4f9ac037e88ee371a",
    remote = "https://github.com/nothings/stb.git",
    shallow_since = "1617298303 -0700",
)

#######################################
# Vulkan

http_archive(
    name = "lib-vulkan-linux",
    build_file = "//:third_party/vulkan/BUILD.linux",
    sha256 = "6d8828fa9c9113ef4083a07994cf0eb13b8d239a5263bd95aa408d2f57585268",
    strip_prefix = "1.2.154.0/x86_64",
    url = "https://sdk.lunarg.com/sdk/download/1.2.154.0/linux/vulkansdk-linux-x86_64-1.2.154.0.tar.gz",
)

use_vulkan_sdk(
    name = "lib-vulkan-osx",
    build_file_abs_path = __workspace_dir__ + "/third_party/vulkan/BUILD.osx",
)

use_vulkan_sdk(
    name = "lib-vulkan-windows",
    build_file_abs_path = __workspace_dir__ + "/third_party/vulkan/BUILD.windows",
)

#######################################
# resource

http_archive(
    name = "resource",
    sha256 = "f1f3ef3d9cda4e9a74171719c3e26aef9b6eeae0b16a9a4f6f3addc1ba2bc131",
    strip_prefix = "resource-1.0.0",
    urls = ["https://github.com/lun0522/resource/archive/1.0.0.zip"],
)
