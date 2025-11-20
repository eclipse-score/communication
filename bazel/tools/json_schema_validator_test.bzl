load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")

# Rule to generate JSON file and schema
def _json_generator_impl(ctx):
    json_file = ctx.actions.declare_file(ctx.attr.name)

    # Assuming you have the content for json and schema
    json_content = ctx.attr.json_content
    ctx.actions.write(json_file, json_content)
    return [DefaultInfo(files = depset([json_file]))]

_json_generator = rule(
    implementation = _json_generator_impl,
    attrs = {
        "json_content": attr.string(),
    },
)

def _testcase(name, json_content, schema_content, expected_failure):
    json_name = name + "_file.json"
    schema_name = name + "_schema.json"

    _json_generator(name = json_name, json_content = json_content)
    _json_generator(name = schema_name, json_content = schema_content)

    validate_json_schema_test(
        name = name,
        expected_failure = expected_failure,
        json = ":" + json_name,
        schema = ":" + schema_name,
    )

def json_schema_validator_tests(name):
    # link testcases to single name
    native.test_suite(
        name = name,
        tests = [
            # positive cases
            ":valid_schema_and_valid_json_test",
            ":valid_schema_with_enabled_additional_props_and_valid_json_test",
            # negative cases
            ":valid_schema_with_disabled_additional_props_and_valid_json_with_extra_property_test",
            ":invalid_schema_and_valid_json_test",
        ],
    )

    # positive case: valid schema and valid json
    _testcase(
        name = "valid_schema_and_valid_json_test",
        json_content = """{ "root": { "obj1": 1, "obj2": "test_string" }  }""",
        schema_content = """{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "type": "object",
    "required": [
        "root"
    ],
    "properties": {
        "root": {
            "type": "object",
            "properties": {
                "obj1": {
                    "type": "integer"
                },
                "obj2": {
                    "type": "string"
                }
            }
        }
    }
}""",
        expected_failure = False,
    )

    # positive case: valid schema, which allows addtional props, and valid json with additional property
    _testcase(
        name = "valid_schema_with_enabled_additional_props_and_valid_json_test",
        json_content = """{ "root": { "obj1": 1, "obj2": "test_string", "uknown_prop": 2 }  }""",
        schema_content = """{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "type": "object",
    "required": [
        "root"
    ],
    "properties": {
        "root": {
            "type": "object",
            "properties": {
                "obj1": {
                    "type": "integer"
                },
                "obj2": {
                    "type": "string"
                }
            }
        }
    }
}""",
        expected_failure = False,
    )

    # negative case: valid schema, which disallow additional props and valid json, but it has disallowed property
    _testcase(
        name = "valid_schema_with_disabled_additional_props_and_valid_json_with_extra_property_test",
        json_content = """{ "root": { "obj1": 1, "obj2": "test_string", "uknown_prop": 2 }  }""",
        schema_content = """{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "type": "object",
    "required": [
        "root"
    ],
    "properties": {
        "root": {
            "type": "object",
            "additionalProperties": false,
            "properties": {
                "obj1": {
                    "type": "integer"
                },
                "obj2": {
                    "type": "string"
                }
            }
        }
    }
}""",
        expected_failure = True,
    )

    # negative case: invalid schema and valid json
    _testcase(
        name = "invalid_schema_and_valid_json_test",
        json_content = """{ "root": { "obj1": 1, "obj2": "test_string", "uknown_prop": 2 }  }""",
        schema_content = """{
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "type": "object",
    "required": [
        "root" this string is invalid
    ]
}""",
        expected_failure = True,
    )
