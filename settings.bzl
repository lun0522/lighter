def compilation_mode():
    native.config_setting(
        name = "optimal_build",
        values = {"compilation_mode": "opt"},
    )
