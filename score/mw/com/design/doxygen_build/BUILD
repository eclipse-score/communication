load("@doxygen//:doxygen.bzl", "doxygen")

doxygen(
    name = "generate_doxygen",
    outs = [
        "html",
        "xml",
    ],
    # \requirement / @requirement is a project-specific tag used to reference
    # requirement IDs. Define it as an xrefitem so doxygen accepts it.
    aliases = [
        "requirement{1}=\\xrefitem requirements \\\"Requirement\\\" \\\"Requirements\\\" \\1",
    ],
    # We only emit HTML and XML; disable LaTeX to avoid the missing epstopdf error.
    generate_latex = "NO",
    project_name = "com",
    # Suppress doxygen progress/info output.
    quiet = "YES",
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//docs/sphinx:__pkg__"],
    # Suppress documentation-quality warnings that reflect source-level issues
    # to be fixed separately:
    # - warn_if_doc_error:      "no matching class member" false positives caused
    #                           by trailing return type syntax in .cpp definitions
    #                           (auto f() -> T) not matching header declarations (T f()),
    #                           and backtick/C-comment parse confusion in flag_file.h
    # - warn_if_incomplete_doc: partially documented parameter lists
    # - warn_no_paramdoc:       completely undocumented parameter lists
    warn_if_doc_error = "NO",
    warn_if_incomplete_doc = "NO",
    warn_no_paramdoc = "NO",
    deps = [
        "//score/mw/com",
    ],
)
