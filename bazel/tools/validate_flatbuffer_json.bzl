"""
Validate a JSON file against a FlatBuffer schema.

This rule converts existing communication JSON files to a FlatBuffer friendly format.
- Convert '-' to '_' in keys (required)
- Convert keys from camelCase to snake_case (avoids warnings)
"""

def _validate_json_flatbuffer_test_impl(ctx):
    """Implementation of the validation test rule."""

    # Create a script that performs the validation
    script = """#!/bin/bash
set -euo pipefail

readonly expected_failure={expected_failure}
readonly converter='{converter}'
readonly flatc='{flatc}'
readonly schema='{schema}'
readonly json='{json}'
readonly tmpdir=$(mktemp -d)

cleanup() {{
    rm -rf "$tmpdir"
}}
trap cleanup EXIT

# Step 1: Convert JSON to FlatBuffer friendly format
"$converter" "$json" "$tmpdir/converted.json"

# Step 2: Validate with flatc by compiling to binary (validates structure)
# Capture both stdout and stderr, suppress output unless there's an error
set +e
output=$("$flatc" --binary -o "$tmpdir" "$schema" "$tmpdir/converted.json" 2>&1)
ret=$?
set -e

if test "$expected_failure" = true && test "$ret" -ne 0; then
    echo "Expected validation to fail, and it did (exit code $ret)."
    echo ""
    echo "FlatBuffer validation errors:"
    echo "$output"
    echo ""
    echo "Test PASSED."
    exit 0
elif test "$expected_failure" = false && test "$ret" -eq 0; then
    echo "Expected validation to succeed, and it did. Test PASSED."
    exit 0
fi

# Test failed - show what went wrong
if test "$ret" -ne 0; then
    echo "FlatBuffer validation errors:"
    echo "$output"
fi

echo ""
echo "Test FAILED: FlatBuffer validation of '$json' against '$schema' exited with code $ret, but expected_failure={expected_failure}"
exit 1
""".format(
        expected_failure = "true" if ctx.attr.expected_failure else "false",
        converter = ctx.executable._converter.short_path,
        flatc = ctx.executable._flatc.short_path,
        schema = ctx.file.schema.short_path,
        json = ctx.file.json.short_path,
    )

    ctx.actions.write(
        output = ctx.outputs.executable,
        content = script,
        is_executable = True,
    )

    runfiles = ctx.runfiles(
        files = [ctx.file.json, ctx.file.schema],
    ).merge(ctx.attr._converter[DefaultInfo].default_runfiles)
    runfiles = runfiles.merge(ctx.attr._flatc[DefaultInfo].default_runfiles)

    return [DefaultInfo(runfiles = runfiles)]

validate_json_flatbuffer_test = rule(
    implementation = _validate_json_flatbuffer_test_impl,
    attrs = {
        "json": attr.label(
            allow_single_file = [".json"],
            mandatory = True,
            doc = "Input JSON file to validate",
        ),
        "schema": attr.label(
            allow_single_file = [".fbs"],
            mandatory = True,
            doc = "FlatBuffer schema file (.fbs) to validate against",
        ),
        "expected_failure": attr.bool(
            default = False,
            doc = "If True, test passes when validation fails (for testing invalid inputs)",
        ),
        "_converter": attr.label(
            default = Label("//bazel/tools:json_to_flatbuffer_json"),
            executable = True,
            cfg = "exec",
        ),
        "_flatc": attr.label(
            default = Label("@flatbuffers//:flatc"),
            executable = True,
            cfg = "exec",
        ),
    },
    test = True,
    doc = """
    Validates a JSON file against a FlatBuffer schema.
    This rule converts existing communication JSON files to a FlatBuffer friendly format.
    
    Example:
        validate_json_flatbuffer_test(
            name = "valid_config_test",
            json = "valid_config.json",
            schema = "ara_com_config.fbs",
        )
        
        validate_json_flatbuffer_test(
            name = "invalid_config_test",
            json = "invalid_config.json",
            schema = "ara_com_config.fbs",
            expected_failure = True,
        )
    """,
)
