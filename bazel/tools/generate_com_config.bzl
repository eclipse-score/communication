"""
Bazel rule to generate FlatBuffer binary configuration from JSON files.

This rule converts existing communication JSON files to a FlatBuffer friendly format.
- Convert '-' to '_' in keys (required)
- Convert keys from camelCase to snake_case (avoids warnings)
"""

def generate_com_config(name, json, convert, visibility = None, **kwargs):
    """
    Generate a FlatBuffer binary configuration file from a JSON input.

    This rule can optionally convert the input JSON to FlatBuffer friendly format
    before compiling to FlatBuffer binary format.

    The schema is hardcoded to the COM FlatBuffer schema at:
    //score/mw/com/impl/configuration:ara_com_config.fbs

    The output .bin file will be generated in the same directory path as the
    input JSON file to match the filegroup behavior for JSON files.

    Args:
        name: Name of the rule (will be used as the target name)
        json: Input JSON configuration file
        convert: Boolean - if True, converts JSON to FlatBuffer friendly format
                 (dash to underscore, camelCase to snake_case). If False, uses
                 JSON as-is (assumes it's already FlatBuffer conform)
        visibility: Visibility for the generated target (optional)

    Outputs:
        A .bin file in the same directory as the input JSON
        Example: "example/ara_com_config.json" -> "example/ara_com_config.bin"

    Example:
        generate_com_config(
            name = "my_config.bin",
            json = "example/config.json",
            convert = True,
            visibility = ["//visibility:public"],
        )
    """

    # Always use the COM FlatBuffer schema
    schema = "//score/mw/com/impl/configuration:ara_com_config.fbs"

    # Preserve the full path of the JSON file, just change extension
    if json.endswith(".json"):
        output_path = json[:-5] + ".bin"
    else:
        output_path = json + ".bin"

    if convert:
        # Intermediate converted JSON file (keep in same directory)
        if json.endswith(".json"):
            converted_json = json[:-5] + "_converted.json"
        else:
            converted_json = json + "_converted.json"

        # Extract just the filename for the flatc intermediate output
        json_filename = json.split("/")[-1]
        if json_filename.endswith(".json"):
            flatc_output = json_filename[:-5] + "_converted.bin"
        else:
            flatc_output = json_filename + "_converted.bin"

        # Step 1: Convert JSON to FlatBuffer friendly format
        native.genrule(
            name = name + "_convert",
            srcs = [json],
            outs = [converted_json],
            tools = ["//bazel/tools:json_to_flatbuffer_json"],
            cmd = "$(location //bazel/tools:json_to_flatbuffer_json) $(SRCS) $@",
            visibility = ["//visibility:private"],
        )

        # Step 2: Compile converted JSON to FlatBuffer binary
        native.genrule(
            name = name,
            srcs = [converted_json, schema],
            outs = [output_path],
            tools = ["@flatbuffers//:flatc"],
            cmd = "$(location @flatbuffers//:flatc) --binary -o $(@D) $(location " + schema + ") $(location " + converted_json + ") && mv $(@D)/" + flatc_output + " $@",
            visibility = visibility,
        )
    else:
        # Extract just the filename for the flatc intermediate output
        json_filename = json.split("/")[-1]
        if json_filename.endswith(".json"):
            flatc_output = json_filename[:-5] + ".bin"
        else:
            flatc_output = json_filename + ".bin"

        # Only move if the output paths differ
        if flatc_output == output_path.split("/")[-1]:
            cmd = "$(location @flatbuffers//:flatc) --binary -o $(@D) $(location " + schema + ") $(location " + json + ")"
        else:
            cmd = "$(location @flatbuffers//:flatc) --binary -o $(@D) $(location " + schema + ") $(location " + json + ") && mv $(@D)/" + flatc_output + " $@"
        
        native.genrule(
            name = name,
            srcs = [json, schema],
            outs = [output_path],
            tools = ["@flatbuffers//:flatc"],
            cmd = cmd,
            visibility = visibility,
        )
