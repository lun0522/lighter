def select_platform(name, osx_target, linux_taregt, headers_target = ""):
    headers = [headers_target] if headers_target else []
    native.cc_library(
        name = name,
        deps = headers + select({
            "@bazel_tools//src/conditions:darwin": [osx_target],
            "@bazel_tools//src/conditions:linux_x86_64": [linux_taregt],
        }),
    )
