load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def assimp_archive(name, build_file, strip_prefix):
    http_archive(
        name = name,
        urls = ["https://github.com/lun0522/lib-assimp/archive/5.0.1.zip"],
        sha256 = "8882c815c924cf9044588db0dae7a396ff6e0952c844d0f165b76582cbbc0ee5",
        strip_prefix = "lib-assimp-5.0.1/" + strip_prefix,
        build_file = build_file,
    )

def freetype_archive(name, build_file, strip_prefix):
    http_archive(
        name = name,
        urls = ["https://github.com/lun0522/lib-freetype/archive/2.10.2.zip"],
        sha256 = "38c4d7c97e8c8b9618b41114d4b957a333e9539ebad6ca82222b283da46d6641",
        strip_prefix = "lib-freetype-2.10.2/" + strip_prefix,
        build_file = build_file,
    )