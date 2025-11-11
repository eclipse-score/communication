"""
Wrapper rule around the expand_template action with support for location
expansion.
"""

def _expand_template_impl(ctx):
    expanded_substitutions = {}
    expanded_substitutions.update(ctx.attr.substitutions)

    for key, expression in ctx.attr.expression_substitutions.items():
        expanded_expression = ctx.expand_location(expression, ctx.attr.deps)

        if expanded_expression == None:
            fail("Failed to expand location: ", expression)

        expanded_substitutions[key] = expanded_expression

    ctx.actions.expand_template(
        template = ctx.file.template,
        output = ctx.outputs.out,
        substitutions = expanded_substitutions,
        is_executable = ctx.attr.is_executable,
    )

expand_template = rule(
    implementation = _expand_template_impl,
    attrs = {
        "deps": attr.label_list(),
        "expression_substitutions": attr.string_dict(default = {}),
        "is_executable": attr.bool(default = False),
        "out": attr.output(mandatory = True),
        "substitutions": attr.string_dict(default = {}),
        "template": attr.label(allow_single_file = True, mandatory = True),
    },
    doc = """
    This rule is a wrapper around the expand_template action, similar to the one
    provided by the bazel skylib[1].
    Additionally it supports location expansion[2,3], which allows for label
    outputs to be correctly mapped in a template through the attribute
    'expression_substitutions'.
    [1] - https://github.com/bazelbuild/bazel-skylib/blob/main/rules/expand_template.bzl
    [2] - https://bazel.build/rules/lib/builtins/ctx#expand_location
    [3] - https://bazel.build/reference/be/make-variables#predefined_label_variables
    """,
)
