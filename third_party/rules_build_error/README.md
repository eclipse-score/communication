# Build-error helper interpreters

The native dependency remains rules_build_error 0.11.0 (CC0-1.0).
`bash-interpreters.patch` adds a Bash shebang to its six Bash helpers.
CodeQL tracing executes these programs directly; without a shebang its shell
fallback rejects `set -o pipefail`. The matcher and expected build-error
semantics remain unchanged. MODULE.bazel applies the patch with strip level 1.

The lang helpers and matcher helpers are all required: changing only the
check entrypoints leaves their directly executed children broken under tracing.
