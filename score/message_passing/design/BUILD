load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "architectural_design",
    "unit_design",
)

architectural_design(
    name = "message_passing_design",
    static = ["client-server.md"],
    visibility = ["//visibility:public"],
)

unit_design(
    name = "client_unit_design",
    visibility = ["//visibility:public"],
)

unit_design(
    name = "server_unit_design",
    visibility = ["//visibility:public"],
)

