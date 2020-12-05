def select_platform(name, osx_target, linux_taregt, include_target = ""):
    include = [include_target] if include_target else []
    native.cc_library(
        name = name,
        deps = include + select({
            "@bazel_tools//src/conditions:darwin": [osx_target],
            "@bazel_tools//src/conditions:linux_x86_64": [linux_taregt],
        }),
    )
