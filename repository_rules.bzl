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
    attrs = {"build_file_abs_path": attr.string(mandatory=True)},
    environ = [_VK_SDK_ENV_VAR],
)

def get_vulkan_include_path(relative_path=""):
    return paths.join(_VK_SDK_SYMLINK, "include", relative_path)

def get_vulkan_lib_path(relative_path):
    return paths.join(_VK_SDK_SYMLINK, "lib", relative_path)

# TODO: rules_foreign_cc doesn't work on Windows yet.
def external_windows_archive(name, strip_prefix, build_file):
    http_archive(
        name = name,
        sha256 = "2e70a4ae84a93820a4d4db4cdd37dd7deb7a1380ad8736d43180d298ef5e04d0",
        strip_prefix = "lighter_external_windows-1.0.0/" + strip_prefix,
        build_file = build_file,
        url = "https://github.com/lun0522/lighter_external_windows/archive/1.0.0.tar.gz",
    )
