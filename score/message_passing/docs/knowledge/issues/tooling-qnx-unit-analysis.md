# QNX Unit Analysis Awaits Tooling Support

Status: blocked
Area: tooling
Scope: Bazel `unit()` QNX build/analysis; tooling pin 3.0.2
Owner: upstream tooling support; component follow-up unassigned
Evidence checked: 2026-11-04; source inspection and explicit project clarification

## Finding

`ClientConnection` supports QNX, as do the native QNX dispatch runtime libraries.
The developing tooling does not yet support QNX builds/analysis through Bazel
`unit()`. Support is expected soon, with no release/date established here.
This is a temporary validation dependency, not missing runtime functionality.

[The BUILD](../../../BUILD) guards `client_connection` and
`qnx_dispatch_{engine,client_factory,server,server_factory}` wrappers with Linux
compatibility. QNX wrappers retain QNX implementation selections, but select no
implementation on Linux. A successful Linux documentation build therefore does
not establish QNX AST/design validation.

## Evidence

[The libclang toolchain](../../../../../bazel/toolchains/BUILD) supplies analysis
flags independently of the QNX compiler. Earlier reproductions failed to find
QNX SDP headers such as `sys/neutrino.h` through that analysis path. The exact
failure is historical, not rerun here. Earlier `allow_parse_errors` experiments
did not replace the need for supported analysis.

Explicitly requesting an incompatible wrapper and skipping it are not successful
analysis. Wildcard builds can skip incompatible wrappers too. Runtime test
wrappers are separate; see [build and test](../build-and-test.md).

## Next Check

On a tooling-version update, check the release and resolved implementation for
QNX `unit()` support. Once supported, verify a focused QNX wrapper build/analysis
with the configured SDK, then review removal of temporary compatibility guards
in a separately scoped change. Record actual results here; update topic pages
only if their guidance or routing changes. Do not remove guards based only on
the expected roadmap.

See [the platform/tooling decision](../decisions/2026-11-04-platform-and-tooling-scope.md).