def generate_target(src_zip):
    native.cc_library(
        name = "assimp_lib",
        deps = [
            ":libassimp",
            ":libIrrXML",
            ":libz",
        ],
        visibility = ["//visibility:public"],
    )

    native.cc_import(
        name = "libassimp",
        static_library = "libassimp.a",
    )

    native.cc_import(
        name = "libIrrXML",
        static_library = "libIrrXML.a",
    )

    native.cc_import(
        name = "libz",
        static_library = "libz.a",
    )

    native.genrule(
        name = "unzip",
        srcs = [src_zip],
        outs = [
            "libassimp.a",
            "libIrrXML.a",
            "libz.a",
        ],
        cmd = "unzip -j -d $(RULEDIR) $<",
        output_to_bindir = True,
    )
