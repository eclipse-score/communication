load("@doxygen//:doxygen.bzl", "doxygen")

doxygen(
    name = "generate_doxygen",
    outs = [
        "html",
        "xml",
    ],
    project_name = "com",
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//docs/sphinx:__pkg__"],
    deps = [
        "//score/mw/com",
    ],
)
