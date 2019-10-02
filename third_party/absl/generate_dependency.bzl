def absl_library(name, hdrs, deps):
    native.cc_library(
        name = name,
        hdrs = hdrs,
        deps = deps,
        strip_include_prefix = "/external/lib-absl",
        include_prefix = "third_party",
        visibility = ["//visibility:public"],
    )