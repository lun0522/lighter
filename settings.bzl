def compilation_mode():
    native.config_setting(
        name = "optimal_build",
        values = {"compilation_mode": "opt"},
    )

def graphics_api():
    native.config_setting(
        name = "use_vulkan",
        values = {"copt": "-DUSE_VULKAN"},
    )
