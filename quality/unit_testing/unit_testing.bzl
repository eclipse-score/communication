load("@rules_cc//cc:defs.bzl", "cc_test")
load("@rules_rust//rust:defs.bzl", "rust_library", "rust_test")
load("@score_qnx_unit_tests//:defs.bzl", "cc_test_qnx", "rust_test_qnx")

def _forwarding_test_impl(ctx):
    actual = ctx.attr.actual
    default_info = actual[DefaultInfo]
    executable = default_info.files_to_run.executable

    # Build the exec command with resolved file paths from forwarded_args
    parts = [executable.short_path]
    for f in ctx.files.forwarded_files:
        parts.append('"{}"'.format(f.short_path))
    parts.append('"$@"')

    out = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.write(
        output = out,
        content = "#!/bin/bash\nexec {}\n".format(" ".join(parts)),
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = [executable] + ctx.files.forwarded_files)
    if default_info.default_runfiles:
        runfiles = runfiles.merge(default_info.default_runfiles)
    if default_info.data_runfiles:
        runfiles = runfiles.merge(default_info.data_runfiles)

    for d in ctx.attr.forwarded_files:
        di = d[DefaultInfo]
        if di.default_runfiles:
            runfiles = runfiles.merge(di.default_runfiles)

    return [DefaultInfo(
        executable = out,
        runfiles = runfiles,
    )]

_forwarding_test = rule(
    implementation = _forwarding_test_impl,
    test = True,
    attrs = {
        "actual": attr.label(
            mandatory = True,
            cfg = "target",
        ),
        "forwarded_files": attr.label_list(
            doc = "Files to pass as positional args to the actual test executable, in order.",
            default = [],
            allow_files = True,
        ),
    },
)

def cc_unit_test(name, target_compatible_with = [], tags = [], **kwargs):
    """
    Macro in order to declare a C++ unit test

    Args:
      name: Target name, to be forwarded to cc_unit_test and transitively to cc_test

      **kwargs: Additional parameters to be forwarded to cc_unit_test and transitively to cc_test. size and timeout
      cannot be provided and if tags is provided, it should not contain the tag unit.
      The following dependencies are already added to deps:
      "@score_baselibs//score/language/safecpp/coverage_termination_handler", and "@googletest//:gtest_main".
      The following toolchain features are already added to features: "aborts_upon_exception".

    Wrapper to create a cc_unit_test that provides the most common dependencies to be used in a unit test of an ASIL B
    application. If for whatever reason you need to have some other common dependency or you cannot follow the restrictions
    of cc_unit_test, do not think about changing this macro to provide an override. Instead, use directly cc_test.

    Like with cc_unit_test, toolchain features for compiler warnings still need to be provided. The reason why it is not
    provided is to prevent that from the outside it looks like for some target we provide and some not. We can only do that
    if we have this option for the other cc_library targets.
    """

    # Avoid mutating input arguments
    deps = [
        "@score_baselibs//score/language/safecpp/coverage_termination_handler",
        "@googletest//:gtest_main",
    ]
    deps += kwargs.pop("deps", [])

    features = [
        "aborts_upon_exception",
    ]
    features += kwargs.pop("features", [])

    if "unit" in tags:
        fail("'unit' tag already provided, please refrain from adding it manually.")

    name_linux = "{}_linux".format(name)
    name_qnx = "{}_qnx".format(name)

    cc_test(
        name = name_linux,
        size = "small",
        timeout = "short",
        features = features,
        deps = deps,
        tags = tags + ["manual"],
        **kwargs
    )

    kwargs.setdefault("visibility", [])

    cc_test_qnx(
        name = name_qnx,
        cc_test = name_linux,
    )

    _forwarding_test(
        name = name,
        actual = select({
            "@platforms//os:qnx": name_qnx,
            "//conditions:default": name_linux,
        }),
        forwarded_files = select({
            "@platforms//os:qnx": [
                "@score_qnx_unit_tests//src:init",
                ":{}_pkg_tar".format(name_qnx),
            ],
            "//conditions:default": [],
        }),
        tags = tags + ["unit"],
        visibility = kwargs["visibility"],
    )

def rust_unit_test(name, target_compatible_with = [], tags = [], **kwargs):
    if "unit" in tags:
        fail("'unit' tag already provided, please refrain from adding it manually.")

    name_linux = "{}_linux".format(name)
    name_qnx = "{}_qnx".format(name)

    rust_test(
        name = name_linux,
        size = "small",
        timeout = "short",
        tags = tags + ["manual"],
        **kwargs
    )

    kwargs.setdefault("visibility", [])

    rust_test_qnx(
        name = name_qnx,
        rust_test = name_linux,
    )

    _forwarding_test(
        name = name,
        actual = select({
            "@platforms//os:qnx": name_qnx,
            "//conditions:default": name_linux,
        }),
        forwarded_files = select({
            "@platforms//os:qnx": [
                "@score_qnx_unit_tests//src:init",
                ":{}_pkg_tar".format(name_qnx),
            ],
            "//conditions:default": [],
        }),
        tags = tags + ["unit"],
        visibility = kwargs["visibility"],
    )
