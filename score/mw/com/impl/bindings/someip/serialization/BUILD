load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "serializer",
    hdrs = ["serializer.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "serializer_adapter",
    hdrs = ["serializer_adapter.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":serializer",
        "//score/mw/com/impl/bindings/someip/serialization/strategy:someip_strategy",
        "@score_baselibs//score/serialization",
    ],
)
