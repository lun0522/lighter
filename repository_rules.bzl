load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

_VK_SDK_ENV_VAR = "VULKAN_SDK"
_VK_SDK_SYMLINK = "vulkan-sdk"

def _use_vulkan_sdk_impl(repository_ctx):
    sdk_path = repository_ctx.os.environ.get(_VK_SDK_ENV_VAR, "")
    if not sdk_path:
        fail("Environment variable '{}' not set".format(_VK_SDK_ENV_VAR))
    repository_ctx.symlink(sdk_path, _VK_SDK_SYMLINK)
    repository_ctx.template("BUILD", repository_ctx.attr.build_file_abs_path)

use_vulkan_sdk = repository_rule(
    implementation = _use_vulkan_sdk_impl,
    local = True,
    attrs = {"build_file_abs_path": attr.string(mandatory = True)},
    environ = [_VK_SDK_ENV_VAR],
)

def get_vulkan_include_path(relative_path = ""):
    return paths.join(_VK_SDK_SYMLINK, "include", relative_path)

def get_vulkan_lib_path(relative_path):
    return paths.join(_VK_SDK_SYMLINK, "lib", relative_path)

_CC_LIBRARY_ALL_SRCS = """cc_library(
    name = "{}",
    hdrs = glob(["**"]),
    include_prefix = "third_party",
    visibility = ["//visibility:public"],
)
"""

def absl_archive(sha256, strip_prefix, url):
    http_archive(
        name = "lib-absl",
        sha256 = sha256,
        strip_prefix = strip_prefix,
        url = url,
    )

    # Since patch commands are executed after the build file is copied there, it would be hard to differentiate that
    # build file from the existing ones in the repository (we want to remove the latter), hence we make
    # 'build_file_content' empty, and we will generate the real one in the patch command.
    build_file_content = _CC_LIBRARY_ALL_SRCS.format("absl_include")
    http_archive(
        name = "lib-absl-include",
        build_file_content = "",
        sha256 = sha256,
        strip_prefix = strip_prefix,
        url = url,
        patch_cmds = [
            "find . -type f ! -name '*.h' -delete",
            "echo -en '{}' > BUILD.bazel".format(build_file_content),
        ],
        patch_cmds_win = [
            "Get-ChildItem -Recurse -Force -File -Exclude *.h | Remove-Item -Confirm:$false -Force",
            "Set-Content -Path BUILD.bazel -Value '{}'".format(build_file_content),
        ],
    )

def gtest_archive(sha256, strip_prefix, url):
    http_archive(
        name = "lib-gtest",
        sha256 = sha256,
        strip_prefix = strip_prefix,
        url = url,
    )

    http_archive(
        name = "lib-gtest-include",
        sha256 = sha256,
        strip_prefix = paths.join(strip_prefix, "googletest/include"),
        url = url,
        build_file_content = _CC_LIBRARY_ALL_SRCS.format("gtest_include"),
    )
