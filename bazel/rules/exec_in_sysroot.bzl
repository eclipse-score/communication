# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

def _merge_default_and_data_runfiles(target, runfiles):
    default_info = target[DefaultInfo]
    if default_info.default_runfiles:
        runfiles = runfiles.merge(default_info.default_runfiles)
    if default_info.data_runfiles:
        runfiles = runfiles.merge(default_info.data_runfiles)
    return runfiles

def _exec_in_sysroot_impl(ctx):
    if len(ctx.files.sysroot) != 1:
        fail("sysroot '{}' must provide exactly one directory artifact".format(ctx.attr.sysroot.label))

    sysroot = ctx.files.sysroot[0]
    sysroot_runfiles_path = sysroot.short_path
    if sysroot_runfiles_path.startswith("../"):
        sysroot_runfiles_path = sysroot_runfiles_path[3:]

    executable_file = ctx.executable.executable
    if executable_file == None:
        fail("executable must provide a runnable target")
    executable_short_path = executable_file.short_path
    if executable_short_path.startswith("../"):
        executable_runfiles_path = executable_short_path[3:]
    else:
        executable_runfiles_path = ctx.workspace_name + "/" + executable_short_path

    out = ctx.actions.declare_file(ctx.label.name)

    # Build exclude paths string - colon-separated list
    exclude_paths = ":".join(ctx.attr.exclude_paths) if ctx.attr.exclude_paths else ""

    wrapper_script = """#!/usr/bin/env bash
set -euo pipefail

# --- begin runfiles.bash initialization ---
if [[ ! -d "${{RUNFILES_DIR:-/dev/null}}" && ! -f "${{RUNFILES_MANIFEST_FILE:-/dev/null}}" ]]; then
  if [[ -f "$0.runfiles_manifest" ]]; then
    export RUNFILES_MANIFEST_FILE="$0.runfiles_manifest"
  elif [[ -f "$0.runfiles/MANIFEST" ]]; then
    export RUNFILES_MANIFEST_FILE="$0.runfiles/MANIFEST"
  elif [[ -f "$0.runfiles/bazel_tools/tools/bash/runfiles/runfiles.bash" ]]; then
    export RUNFILES_DIR="$0.runfiles"
  fi
fi
if [[ -f "${{RUNFILES_DIR:-/dev/null}}/bazel_tools/tools/bash/runfiles/runfiles.bash" ]]; then
  source "${{RUNFILES_DIR}}/bazel_tools/tools/bash/runfiles/runfiles.bash"
elif [[ -f "${{RUNFILES_MANIFEST_FILE:-/dev/null}}" ]]; then
  source "$(grep -m1 '^bazel_tools/tools/bash/runfiles/runfiles.bash ' "$RUNFILES_MANIFEST_FILE" | cut -d ' ' -f 2-)"
else
  echo >&2 "ERROR: cannot find @bazel_tools//tools/bash/runfiles:runfiles.bash"
  exit 1
fi
# --- end runfiles.bash initialization ---

FAKECHROOT_WRAPPER="$(rlocation '{wrapper_short_path}')"
SYSROOT_DIR="$(rlocation '{sysroot_short_path}')"
EXECUTABLE_FILE="$(rlocation '{executable_runfiles_path}')"

if [[ -z "${{FAKECHROOT_WRAPPER}}" || ! -x "${{FAKECHROOT_WRAPPER}}" ]]; then
  echo "ERROR: could not resolve fakechroot wrapper: {wrapper_short_path}" >&2
  exit 1
fi

if [[ -z "${{SYSROOT_DIR}}" || ! -d "${{SYSROOT_DIR}}" ]]; then
  echo "ERROR: could not resolve sysroot directory: {sysroot_short_path}" >&2
  exit 1
fi

if [[ ! -x "${{SYSROOT_DIR}}/usr/bin/fakechroot" ]]; then
  echo "ERROR: sysroot does not provide /usr/bin/fakechroot: ${{SYSROOT_DIR}}" >&2
  exit 1
fi

if [[ -z "${{EXECUTABLE_FILE}}" || ! -f "${{EXECUTABLE_FILE}}" ]]; then
  echo "ERROR: could not resolve executable target: {executable_runfiles_path}" >&2
  exit 1
fi

export SYSROOT_DIR
if [[ -n "{exclude_paths}" ]]; then
  export FAKECHROOT_EXCLUDE_PATH="{exclude_paths}"
fi

# The executable lives in host runfiles, not in the sysroot. Exclude its path so
# chrooted execution can still open and execute it.
EXECUTABLE_DIR="$(dirname "${{EXECUTABLE_FILE}}")"
if [[ -n "${{FAKECHROOT_EXCLUDE_PATH:-}}" ]]; then
  export FAKECHROOT_EXCLUDE_PATH="${{EXECUTABLE_DIR}}:${{EXECUTABLE_FILE}}:${{FAKECHROOT_EXCLUDE_PATH}}"
else
  export FAKECHROOT_EXCLUDE_PATH="${{EXECUTABLE_DIR}}:${{EXECUTABLE_FILE}}"
fi

exec "${{FAKECHROOT_WRAPPER}}" "${{EXECUTABLE_FILE}}" "$@"
""".format(
        wrapper_short_path = ctx.workspace_name + "/" + ctx.executable._fakechroot_wrapper.short_path,
        sysroot_short_path = sysroot_runfiles_path,
        executable_runfiles_path = executable_runfiles_path,
        exclude_paths = exclude_paths,
    )
    ctx.actions.write(output = out, content = wrapper_script, is_executable = True)

    runfiles = ctx.runfiles(
        files = [out, ctx.executable._fakechroot_wrapper, sysroot, executable_file] + ctx.files._bash_runfiles,
    )
    runfiles = _merge_default_and_data_runfiles(ctx.attr.executable, runfiles)
    runfiles = _merge_default_and_data_runfiles(ctx.attr._fakechroot_wrapper, runfiles)
    runfiles = _merge_default_and_data_runfiles(ctx.attr._bash_runfiles, runfiles)
    runfiles = _merge_default_and_data_runfiles(ctx.attr.sysroot, runfiles)

    return [DefaultInfo(
        executable = out,
        files = depset([out]),
        runfiles = runfiles,
    )]


exec_in_sysroot = rule(
    implementation = _exec_in_sysroot_impl,
    executable = True,
    attrs = {
        "executable": attr.label(mandatory = True, executable = True, cfg = "exec"),
        "sysroot": attr.label(mandatory = True, allow_files = True),
        "exclude_paths": attr.string_list(
            default = [],
            doc = "Paths to exclude from fakechroot (colon-separated in the env var)",
        ),
        "_bash_runfiles": attr.label(
            default = Label("@bazel_tools//tools/bash/runfiles"),
            allow_files = True,
        ),
        "_fakechroot_wrapper": attr.label(
            default = Label("//bazel/rules:exec_in_sysroot"),
            executable = True,
            cfg = "exec",
        ),
    },
    doc = """
    Produces an executable wrapper that runs a given executable target using the supplied sysroot.
    The wrapped executable runs within fakechroot, allowing access to sysroot tools.
    """,
)
