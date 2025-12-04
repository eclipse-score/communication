load(":json_schema_validator_test.bzl", "json_schema_validator_tests")

json_schema_validator_tests(name = "json_validator_tests")

py_binary(
    name = "json_to_flatbuffer_json",
    srcs = ["json_to_flatbuffer_json.py"],
    visibility = ["//visibility:public"],
)
