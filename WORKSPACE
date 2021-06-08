workspace(name = "lighter")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "bazel_skylib",
    sha256 = "1c531376ac7e5a180e0237938a2536de0c54d93f5c278634818e0efc952dd56c",
    urls = ["https://github.com/bazelbuild/bazel-skylib/archive/1.0.3.tar.gz"],
)

load("@//:repository_rules.bzl", "absl_archive", "external_windows_archive", "use_vulkan_sdk")
load("@bazel_skylib//lib:paths.bzl", "paths")

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

absl_archive(
    sha256 = "441db7c09a0565376ecacf0085b2d4c2bbedde6115d7773551bc116212c2a8d6",
    url = "https://github.com/abseil/abseil-cpp/archive/20210324.1.tar.gz",
    strip_prefix = "abseil-cpp-20210324.1",
)

#######################################
# Assimp

http_archive(
    name = "lib-assimp",
    sha256 = "11310ec1f2ad2cd46b95ba88faca8f7aaa1efe9aa12605c55e3de2b977b3dbfc",
    strip_prefix = "assimp-5.0.1",
    build_file = "//:third_party/BUILD.assimp",
    url = "https://github.com/assimp/assimp/archive/v5.0.1.tar.gz",
)

external_windows_archive(
    name = "lib-assimp-windows",
    strip_prefix = "assimp",
    build_file = "//:third_party/BUILD.assimp.windows",
)

#######################################
# FreeType

http_archive(
    name = "lib-freetype",
    sha256 = "5eab795ebb23ac77001cfb68b7d4d50b5d6c7469247b0b01b2c953269f658dac",
    strip_prefix = "freetype-2.10.4",
    build_file = "//:third_party/BUILD.freetype",
    url = "https://download.savannah.gnu.org/releases/freetype/freetype-2.10.4.tar.gz",
)

external_windows_archive(
    name = "lib-freetype-windows",
    strip_prefix = "freetype",
    build_file = "//:third_party/BUILD.freetype.windows",
)

#######################################
# GLAD

http_archive(
    name = "lib-glad",
    build_file = "//:third_party/BUILD.glad",
    sha256 = "fb70142b175aff3fc9cc0438ea4b2047c6d37163a7afe6942a6868755ec3e636",
    strip_prefix = "lib-glad-4.5",
    url = "https://github.com/lun0522/lib-glad/archive/4.5.tar.gz",
)

#######################################
# GLFW

http_archive(
    name = "lib-glfw",
    sha256 = "cc8ac1d024a0de5fd6f68c4133af77e1918261396319c24fd697775a6bc93b63",
    strip_prefix = "glfw-3.3.4",
    build_file = "//:third_party/BUILD.glfw",
    url = "https://github.com/glfw/glfw/archive/3.3.4.tar.gz",
)

external_windows_archive(
    name = "lib-glfw-windows",
    strip_prefix = "glfw",
    build_file = "//:third_party/BUILD.glfw.windows",
)

#######################################
# GLM

http_archive(
    name = "lib-glm",
    build_file = "//:third_party/BUILD.glm",
    sha256 = "7d508ab72cb5d43227a3711420f06ff99b0a0cb63ee2f93631b162bfe1fe9592",
    strip_prefix = "glm-0.9.9.8",
    url = "https://github.com/g-truc/glm/archive/0.9.9.8.tar.gz",
)

#######################################
# GoogleTest

http_archive(
    name = "lib-googletest",
    sha256 = "94c634d499558a76fa649edb13721dce6e98fb1e7018dfaeba3cd7a083945e91",
    strip_prefix = "googletest-release-1.10.0",
    url = "https://github.com/google/googletest/archive/release-1.10.0.tar.gz",
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
    name = "lib-shaderc",
    build_file_abs_path = paths.join(__workspace_dir__, "third_party/BUILD.shaderc"),
)

#######################################
# SPIRV-Cross

http_archive(
    name = "lib-spirv-cross",
    sha256 = "d700863b548cbc7f27a678cee305f561669a126eb2cc11d36a7023dfc462b9c4",
    strip_prefix = "SPIRV-Cross-2021-01-15",
    build_file = "//:third_party/BUILD.spirv_cross",
    url = "https://github.com/KhronosGroup/SPIRV-Cross/archive/2021-01-15.tar.gz",
)

external_windows_archive(
    name = "lib-spirv-cross-windows",
    strip_prefix = "spirv_cross",
    build_file = "//:third_party/BUILD.spirv_cross.windows",
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

use_vulkan_sdk(
    name = "lib-vulkan",
    build_file_abs_path = paths.join(__workspace_dir__, "third_party/BUILD.vulkan"),
)

#######################################
# resource

http_archive(
    name = "resource",
    sha256 = "9a1977db13a18d7af8c0925e12084b0597d1b1c55a912101699bbf58075f4272",
    strip_prefix = "resource-1.0.0",
    url = "https://github.com/lun0522/resource/archive/1.0.0.tar.gz",
)
