load("@trlc//:trlc.bzl", "trlc_requirements_test", "trlc_specification")

trlc_specification(
    name = "requirement_metamodel",
    srcs = [
        "score_requirements_model.rsl",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements_test(
    name = "requirement_metamodel_test",
    reqs = [
        ":requirement_metamodel",
    ],
)
