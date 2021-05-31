load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def assimp_archive(name, build_file):
    http_archive(
        name = name,
        urls = ["https://github.com/assimp/assimp/archive/refs/tags/v5.0.1.zip"],
        sha256 = "d10542c95e3e05dece4d97bb273eba2dfeeedb37a78fb3417fd4d5e94d879192",
        strip_prefix = "assimp-5.0.1",
        build_file = build_file,
    )

def freetype_archive(name, build_file):
    http_archive(
        name = name,
        urls = ["https://download.savannah.gnu.org/releases/freetype/freetype-2.10.4.tar.gz"],
        sha256 = "5eab795ebb23ac77001cfb68b7d4d50b5d6c7469247b0b01b2c953269f658dac",
        strip_prefix = "freetype-2.10.4",
        build_file = build_file,
    )

def glfw_archive(name, build_file):
    http_archive(
        name = name,
        urls = ["https://github.com/glfw/glfw/archive/3.3.4.zip"],
        sha256 = "19a1048439a35e49f9b48fbe2e42787cfabae70df80ffd096b3b553bbd8a09f7",
        strip_prefix = "glfw-3.3.4",
        build_file = build_file,
    )

def spirv_cross_archive(platform, url, sha256):
    http_archive(
        name = "lib-spirv-cross-" + platform,
        urls = [url],
        sha256 = sha256,
        build_file = "//:third_party/BUILD.spirv_cross",
    )
