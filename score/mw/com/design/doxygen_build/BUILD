load("@doxygen//:doxygen.bzl", "doxygen")

doxygen(
    name = "generate_doxygen",
    outs = [
        "html",
        "xml",
    ],
    project_name = "com",
    visibility = ["//score/mw/com/doc/sphinx:__pkg__"],
    deps = [
        "//score/mw/com",
    ],
)
