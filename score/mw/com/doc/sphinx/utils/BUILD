load("@rules_python//python:defs.bzl", "py_binary", "py_library")

py_library(
    name = "sphinx_build_wrapper",
    srcs = ["sphinx_build_wrapper.py"],
    visibility = ["//score/mw/com/doc/sphinx:__pkg__"],
)

py_binary(
    name = "generate_api_rst_bin",
    srcs = ["extract_api_items.py"],
    main = "extract_api_items.py",
    visibility = ["//score/mw/com/doc/sphinx:__pkg__"],
)
