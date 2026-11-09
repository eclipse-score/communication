load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")

exports_files(
    ["html_integrity.py"],
    visibility = ["//visibility:public"],
)

py_library(
    name = "html_integrity",
    srcs = ["html_integrity.py"],
    imports = ["."],
)

py_binary(
    name = "html_integrity_check",
    srcs = ["html_integrity.py"],
    visibility = ["//visibility:public"],
)

py_binary(
    name = "plantuml_render_runner",
    srcs = ["plantuml_render_runner.py"],
    visibility = ["//visibility:public"],
)

py_library(
    name = "plantuml_render_lib",
    srcs = ["plantuml_render_runner.py"],
    imports = ["."],
)

py_test(
    name = "plantuml_render_runner_test",
    srcs = ["plantuml_render_runner_test.py"],
    deps = [":plantuml_render_lib"],
)

py_test(
    name = "html_integrity_test",
    srcs = ["html_integrity_test.py"],
    deps = [":html_integrity"],
)
