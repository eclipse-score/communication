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
    deps = [
        "//score/mw/com",
    ],
)
