load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def assimp_archive(name, build_file, strip_prefix):
    http_archive(
        name = name,
        urls = ["https://github.com/lun0522/lib-assimp/archive/5.0.1.zip"],
        sha256 = "5ac7ce805a5267fd0dad473e349576185fb7a159e999a39aa5f3a75120e2efe8",
        strip_prefix = "lib-assimp-5.0.1/" + strip_prefix,
        build_file = build_file,
    )

def freetype_archive(name, build_file, strip_prefix):
    http_archive(
        name = name,
        urls = ["https://github.com/lun0522/lib-freetype/archive/2.10.2.zip"],
        sha256 = "ed3ba2523de8550bab4a6ada61da7f5c440e6ab1985edfc6f538a4503c8e678d",
        strip_prefix = "lib-freetype-2.10.2/" + strip_prefix,
        build_file = build_file,
    )

def glfw_archive(name, build_file, strip_prefix):
    http_archive(
        name = name,
        urls = ["https://github.com/lun0522/lib-glfw/archive/3.3.2.zip"],
        sha256 = "3f7bf26e3e0323c3f798558fd6b23c67527e095fb14e24c598e67c56db4a94a1",
        strip_prefix = "lib-glfw-3.3.2/" + strip_prefix,
        build_file = build_file,
    )
