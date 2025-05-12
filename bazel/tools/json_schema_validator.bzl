"""
Creates "validate_json_schema_test" test rule which validates the input "json" file against its "schema"
"""

def _impl(ctx):
    script = """
    readonly expected_failure={expected_failure}
    '{validator}' '{schema}' < '{json}'
    readonly ret=$?
    if test "$expected_failure" = true && test "$ret" -ne 0; then
        exit 0
    elif test "$expected_failure" = false && test "$ret" -eq 0; then
        exit 0
    fi
    echo "Test Failed: Json validator performed json validation of '{json}' against '{schema}' schema with exit code $ret, meanwhile rule attribute 'expected_failure' is equal to '{expected_failure}'.
This means in case expected_failure=false then expected exit code should be zero (valid json). Otherwise, if expected_failure=true then exit code is non-zero (invalid json).
Note: by default expected_failure is false."
    exit 1
    """.format(
        validator = ctx.attr._validator.files_to_run.executable.short_path,
        schema = ctx.file.schema.short_path,
        json = ctx.file.json.short_path,
        expected_failure = "true" if ctx.attr.expected_failure else "false",
    )

    ctx.actions.write(
        output = ctx.outputs.executable,
        content = script,
    )

    # To ensure the files needed by the script are available, put them in the runfiles
    runfiles = ctx.runfiles(files = [ctx.file._validator, ctx.file.json, ctx.file.schema])
    return [DefaultInfo(runfiles = runfiles)]

validate_json_schema_test = rule(
    implementation = _impl,
    attrs = {
        "expected_failure": attr.bool(default = False),
        "json": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
        "schema": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
        "_validator": attr.label(
            default = Label("@json_schema_validator"),
            allow_single_file = True,
            executable = True,
            cfg = "exec",
        ),
    },
    test = True,
)
