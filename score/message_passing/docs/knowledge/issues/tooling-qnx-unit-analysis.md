<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# QNX Unit Analysis Awaits Tooling Support

Status: blocked
Area: tooling
Scope: Bazel `unit()` QNX build/analysis; tooling pin 3.0.2
Owner: upstream tooling support; component follow-up unassigned
Evidence checked: 2026-11-17; wrapper/rule inspection and VM-backed runtime tests.
Original platform/tooling clarification: 2026-11-04.

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

The 2026-11-17 engine-design baseline executed the public `qnx_dispatch_test`,
`timed_command_queue_test`, and `qnx_resource_path_test` targets with
`--config=qnx`. The infrastructure built the guest image and started the VM;
the guest identified QNX OS 8.0 on x86_64, and all 54/6/4 cases passed.
This confirms runtime execution is available independently of `unit()` analysis.
The [engine design](../../../dependability/software_unit_design/qnx_dispatch_engine.rst)
records selected behavior; the [verification inventory](../qnx-dispatch-engine-verification.md)
owns test scope and remaining checks.
The blocked analysis status and compatibility guards are unchanged.

## Next Check

On a tooling-version update, check the release and resolved implementation for
QNX `unit()` support. Once supported, verify a focused QNX wrapper build/analysis
with the configured SDK, then review removal of temporary compatibility guards
in a separately scoped change. Record actual results here; update topic pages
only if their guidance or routing changes. Do not remove guards based only on
the expected roadmap.

See [the platform/tooling decision](../decisions/2026-11-04-platform-and-tooling-scope.md).