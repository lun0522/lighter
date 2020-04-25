load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")

def assimp_repository(name, build_file, strip_prefix = ""):
    new_git_repository(
        name = name,
        remote = "https://github.com/lun0522/lib-assimp.git",
        commit = "8fb0e23248453dff1a70e2d0a040f5f8803231dd",
        shallow_since = "1587837826 -0700",
        strip_prefix = strip_prefix,
        build_file = build_file,
    )

def freetype_repository(name, build_file, strip_prefix = ""):
    new_git_repository(
        name = name,
        remote = "https://github.com/lun0522/lib-freetype.git",
        commit = "dd7140f9af5eec2c6c595cfd6aac585d2652c061",
        shallow_since = "1587848025 -0700",
        strip_prefix = strip_prefix,
        build_file = build_file,
    )