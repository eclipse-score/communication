<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Complete the Safety Argument and Traceability

Status: open
Area: safety
Scope: current message-passing safety artefacts; tooling 3.0.2
Owner: safety and requirements reviewers; assignment pending
Evidence checked: 2026-11-04; source inspection, not reproduced test outcomes

## Findings

- [DummyAssessment](../../../dependability/safety_analysis/assessments.trlc)
  covers `message_passing_fta.OsIpcFaultHandling` with `measures = []` and says
  "No assessment performed". That is not a justified non-applicability argument.
  Other FTA causes need real applicability/sufficiency review too.
- [Failure modes](../../../dependability/safety_analysis/failure_modes.trlc)
  have no `interface` assignments. Their
  [BUILD target](../../../dependability/safety_analysis/BUILD) also lacks an
  architectural-design dependency for generated `ApiMethod` references. Typed
  references must replace the removed string mappings when content is agreed.
- [Safety-measure requirements](../../../dependability/safety_analysis/safetymeasures.trlc)
  still pin feature parents at `@1`. Affected records are `OsIpcFaultHandling`,
  `SendBufferArgumentValidation`, `BE_MessageTooBig`, and `BE_SendQueueExhausted`.
  Current [feature versions](../../../dependability/requirements/feature_requirements.trlc)
  include `ServerInterface@2`, `OSIndependentAPI@2`,
  `SynchronousUnidirectionalCommunication@3`,
  `SynchronousBidirectionalCommunication@2`,
  `AsynchronousUnidirectionalCommunication@3`, and
  `AllowsBoundedMonotonicMemoryAllocation@2`. Re-pin after semantic review,
  not merely because the newer number exists.
- The safety-measure target is supplied to safety analysis, but no component
  allocates it in `requirements`; [component_message_passing](../../../dependability/BUILD)
  allocates only the regular component-requirement target. Existing
  [client tests](../../../client_connection_test.cpp) and
  [QNX engine tests](../../../qnx_dispatch_engine_test.cpp) nevertheless trace
  to safety records. This is a wiring gap, not proof that no tests exist.

These are source facts, not a new failing-test report or an exhaustive review
of root-cause/API/test coverage. Historical counts are intentionally omitted.

## Next Check

When a safety-analysis change is explicitly scheduled, agree the public-API
failure analysis and QNX scope first. Re-derive causes from the real QNX paths,
confirm applicability/containment with the safety reviewer, and only then wire
typed API references, assessments, agreed measures/AoUs, allocation, and tests.

Use [the safety guidance](../safety-analysis.md) and
[explicit validation targets](../build-and-test.md#explicit-seooc-checks-and-reports).
Record fresh results with the tooling version. Manual tags and development
maturity must not be interpreted as completeness, or removed solely to hide a
failure. Do not fix placeholder content as an incidental documentation task.