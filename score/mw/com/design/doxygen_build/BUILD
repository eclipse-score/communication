load("@doxygen//:doxygen.bzl", "doxygen")

doxygen(
    name = "generate_doxygen",
    outs = [
        "html",
        "xml",
    ],
    project_name = "com",
    # Building documentation is resource-intensive,so it will be handled by dedicated jobs instead of during full platform builds.
    tags = ["manual"],
    visibility = ["//score/mw/com/doc/sphinx:__pkg__"],
    deps = [
        "//score/mw/com",
    ],
)
