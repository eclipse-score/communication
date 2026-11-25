"""Small reusable rendering and HTML integrity checks for documentation."""

load("@rules_python//python:defs.bzl", "py_test")

visibility(["//..."])

def _font_file(files, suffix):
    matches = [file for file in files if file.basename.endswith(suffix)]
    if len(matches) != 1:
        fail("Expected one PlantUML font resource ending in %s" % suffix)
    return matches[0]

def _plantuml_render_impl(ctx):
    output = ctx.actions.declare_directory(ctx.label.name)
    args = ctx.actions.args()
    args.add("--output", output.path)
    args.add("--format", ctx.attr.format)
    args.add("--plantuml", ctx.executable._plantuml.path)
    args.add("--graphviz", ctx.executable._graphviz.path)
    args.add("--font-template", _font_file(ctx.files._fonts, ".tpl").path)
    args.add("--font", _font_file(ctx.files._fonts, ".ttf").path)
    for source in ctx.files.srcs:
        args.add("--source")
        args.add(source.short_path)
        args.add(source.path)
    for include in ctx.files.includes:
        args.add("--include")
        args.add(include.short_path)
        args.add(include.path)
    ctx.actions.run(
        executable = ctx.executable._runner,
        arguments = [args],
        inputs = depset(ctx.files.srcs + ctx.files.includes + ctx.files._fonts),
        outputs = [output],
        tools = [
            ctx.attr._runner.files_to_run,
            ctx.attr._plantuml.files_to_run,
            ctx.attr._graphviz.files_to_run,
        ],
        mnemonic = "PlantumlRender",
        progress_message = "Rendering PlantUML for %s" % ctx.label,
    )
    return [DefaultInfo(files = depset([output]))]

plantuml_render = rule(
    implementation = _plantuml_render_impl,
    doc = "Render one diagram per source with pinned PlantUML, fonts, and Graphviz.",
    attrs = {
        "srcs": attr.label_list(allow_files = [".puml"], mandatory = True),
        "includes": attr.label_list(allow_files = True),
        "format": attr.string(default = "svg", values = ["svg", "png"]),
        "_runner": attr.label(
            default = Label(":plantuml_render_runner"),
            executable = True,
            cfg = "exec",
        ),
        "_plantuml": attr.label(
            default = Label("@score_tooling//third_party/plantuml:plantuml"),
            executable = True,
            cfg = "exec",
        ),
        "_graphviz": attr.label(
            default = Label("@score_tooling//third_party/docs_runtime:dot"),
            executable = True,
            cfg = "exec",
        ),
        "_fonts": attr.label(
            default = Label("@score_tooling//third_party/plantuml:fontconfig_fallback"),
            allow_files = True,
        ),
    },
)

def html_integrity_test(name, site, expectations, **kwargs):
    """Check a Sphinx HTML tree against separately declared page contracts."""
    py_test(
        name = name,
        srcs = [Label(":html_integrity.py")],
        main = Label(":html_integrity.py"),
        data = [site, expectations],
        args = [
            "--site",
            "$(location %s)" % site,
            "--expectations",
            "$(location %s)" % expectations,
        ],
        **kwargs
    )
