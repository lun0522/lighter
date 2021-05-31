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
