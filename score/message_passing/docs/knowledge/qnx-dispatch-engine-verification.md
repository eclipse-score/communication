<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# QNX Dispatch Engine Verification Evidence

Read when interpreting engine test coverage, development evidence or the
distinction between documentation checks and QNX implementation analysis.
Scope: the [engine unit design](../../dependability/software_unit_design/qnx_dispatch_engine.rst).
Evidence: source review and the recorded pilot runs below, not new fault
reproductions from documentation cleanup.

## Source Review Boundary

The pilot inspected the [engine](../../qnx_dispatch/qnx_dispatch_engine.cpp),
[runner](../../qnx_dispatch/dispatch_thread_runner.cpp),
[timed queue](../../timed_command_queue.cpp), their declarations, the
[engine interface](../../i_shared_resource_engine.h) and
[future helper](../../non_allocating_future/non_allocating_future.h).
The [concrete server](../../qnx_dispatch/qnx_dispatch_server.cpp) establishes
adapter subclasses and separate reply/notification storage, not an expansion
of the engine unit's ownership boundary.

## Existing Test Evidence

| Source/cases | Scope and remaining limits |
| --- | --- |
| [Runner tests](../../dispatch_thread_runner_test.cpp) | Unstarted lifetime, successful startup/destruction and four checked setup-failure death cases through OSAL mocks. No concurrent-stop or restart-after-stop qualification. |
| [Engine tests](../../qnx_dispatch_engine_test.cpp) | Setup failures, construction/destruction with a dispatch-block error, endpoint/resource-manager/protocol cases and null/nonmatching owner cleanup. The cleanup smoke case is not matching-owner cancellation or a submission-barrier test. |
| `ServerStart_Unsuccessful`, `ServerStartStop` | Attachment error and start/stop behavior. |
| `ServerOpenCheckFailure`, `ServerOpenCheckSuccess`, `ServerOpenCheckSuccessConnectionAttached` | Open verification errors, attribute-lock scope and OCB binding. |
| `ServerWriteChecks*`, `ServerReadChecks*`, `ServerIoMsg*` | Native adapter guards/delegation through OSAL mocks, not exhaustive native callback/lifetime qualification. |
| `PosixEndpoint` | Register/unregister and disconnect completion. |
| `PosixEndpointEventFailures` | Two event-registration error logs and one send-error log while removal still runs; not the complete client's readiness and later delivery. |
| `PosixEndpointCoidDeathPulse` | Nonmatching, obsolete and different-ID lookup results. Its input callback explicitly unregisters the endpoint, producing the later disconnect callback. |
| `SendProtocolMessageFailure`, `ReceiveProtocolMessageFailure` | Write errors, read errors and zero-byte `EPIPE`; not exhaustive short-write or borrowed-buffer lifetime coverage. |
| [Timed queue tests](../../timed_command_queue_test.cpp) | Immediate/timed execution, equal-deadline ordering, owner-key cleanup/capture destruction and null-key distinction, not native timer delivery. |
| [Resource-path tests](../../qnx_resource_path_test.cpp) | Identifier bounds/path formation, not QNX transport/dispatch. |

## Recorded Runtime Baselines

A fresh, non-cached VM run on 2026-11-19 passed all 54 cases in
`qnx_dispatch_test`. The supporting QNX baseline on 2026-11-17 passed 6
`timed_command_queue_test` and 4 `qnx_resource_path_test` cases.
Guest logs identified QNX OS 8.0 on x86_64.

These are retained run results, not executions repeated by research relocation
or fault-injection proofs for the new review records. The dispatch suite mixes
OSAL-mocked and backend integration cases. From the repository root, with the
configured SDK/runner prerequisites:

```sh
bazel test --config=qnx //score/message_passing:qnx_dispatch_test //score/message_passing:timed_command_queue_test //score/message_passing:qnx_resource_path_test
```

Public forwarding targets build the guest image and launch the VM. Inspect the
guest log and XML, not just the number of Bazel test targets.

## Documentation and Implementation Checks Are Separate

```sh
bazel build //score/message_passing:qnx_dispatch_engine //score/message_passing/dependability/software_unit_design:qnx_dispatch_engine_render
bazel test //score/message_passing/dependability/software_unit_design:qnx_dispatch_engine_html_test
```

The wrapper consumes diagram-parser outputs; the render target uses real
PlantUML. The HTML contract checks published views and caption navigation.
None proves C++ implementation fidelity or complete behavioral coverage.

With the 3.0.2 tooling pin, the Linux-only wrapper selects no implementation on
Linux and is incompatible with QNX. The observed `validation.log` reports
`SKIPPED` because required inputs are absent. Its test list also remains empty;
this inventory adds no requirement-coverage claim or QNX-test attachment to
the Linux wrapper. See [the QNX analysis record](issues/tooling-qnx-unit-analysis.md).

The source-level descriptive inventory covers both engine paths, not complete
qualification. Independent review records track
[timer rearming](issues/functionality-qnx-overdue-timer-rearming.md),
[progress errors](issues/functionality-qnx-dispatch-progress-errors.md),
[registration failure policy](issues/functionality-qnx-endpoint-registration-failures.md)
and [worker confinement](issues/functionality-qnx-client-worker-confinement.md).
Matching-owner cancellation/reentrant submission, endpoint/pulse and borrowed-
buffer lifetime, timing and shutdown contexts also require focused evidence.
Passing ordinary tests does not resolve those questions or establish safety acceptance.
