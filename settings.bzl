def graphics_api():
    native.config_setting(
        name = "use_vulkan",
        values = {"copt": "-DUSE_VULKAN"},
    )