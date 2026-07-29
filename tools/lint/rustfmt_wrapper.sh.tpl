#!/usr/bin/env bash
# Wrapper around the rules_rust rustfmt binary that injects the shared policy.
set -euo pipefail

# --- begin runfiles.bash initialization v3 ---
# Copy-pasted from the Bazel Bash runfiles library v3.
# https://github.com/bazelbuild/bazel/blob/master/tools/bash/runfiles/runfiles.bash
f=bazel_tools/tools/bash/runfiles/runfiles.bash
source "${RUNFILES_DIR:-/dev/null}/$f" 2>/dev/null || \
source "$(grep -sm1 "^$f " "${RUNFILES_MANIFEST_FILE:-/dev/null}" | cut -f2- -d' ')" 2>/dev/null || \
source "$0.runfiles/$f" 2>/dev/null || \
source "$(grep -sm1 "^$f " "$0.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
source "$(grep -sm1 "^$f " "$0.exe.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
{ echo >&2 "ERROR: runfiles.bash initializer cannot find $f"; exit 1; }
# --- end runfiles.bash initialization v3 ---

config="@@RUSTFMT_TOML@@"
rustfmt="@@RUSTFMT_BIN@@"

exec "$(rlocation "$rustfmt")" --config-path "$(rlocation "$config")" "$@"