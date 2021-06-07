load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

_VK_SDK_ENV_VAR = "VULKAN_SDK"

def _use_vulkan_sdk_impl(repository_ctx):
    sdk_path = repository_ctx.os.environ.get(_VK_SDK_ENV_VAR, "")
    if not sdk_path:
        fail("Environment variable '{}' not set".format(_VK_SDK_ENV_VAR))
    repository_ctx.symlink(sdk_path, "vulkan-sdk")
    repository_ctx.template("BUILD", repository_ctx.attr.build_file_abs_path)

use_vulkan_sdk = repository_rule(
    implementation = _use_vulkan_sdk_impl,
    local = True,
    attrs = {"build_file_abs_path": attr.string(mandatory=True)},
    environ = [_VK_SDK_ENV_VAR],
)

# TODO: rules_foreign_cc doesn't work on Windows yet.
def external_windows_archive(name, build_file):
    http_archive(
        name = name,
        sha256 = "e4863252da6e7291090e71239d8dd69da5150b2067add6e285b7c60271f83ed7",
        strip_prefix = "lighter_external_windows-1.0.0",
        build_file = build_file,
        url = "https://github.com/lun0522/lighter_external_windows/archive/1.0.0.tar.gz",
    )
