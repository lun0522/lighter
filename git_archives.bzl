load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def assimp_archive(name, build_file, strip_prefix = ""):
    http_archive(
        name = name,
        urls = ["https://github.com/lun0522/lib-assimp/archive/5.0.1.zip"],
        sha256 = "46dd2f4575ba93482b70bf34b343258f4e7e1d8a190174716871e737178ae9d8",
        strip_prefix = "lib-assimp-5.0.1/" + strip_prefix,
        build_file = build_file,
    )

def freetype_archive(name, build_file, strip_prefix = ""):
    http_archive(
        name = name,
        urls = ["https://github.com/lun0522/lib-freetype/archive/2.10.1.zip"],
        sha256 = "094dcc41a5397435f9596663e01ebe1950bdec1ff3c870787f0d45a2695dca29",
        strip_prefix = "lib-freetype-2.10.1/" + strip_prefix,
        build_file = build_file,
    )