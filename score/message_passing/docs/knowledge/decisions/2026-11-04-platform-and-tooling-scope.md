<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Separate Platform Support from Tooling Readiness

Status: accepted
Date: 2026-11-04
Scope: message-passing implementation support and SEooC verification evidence

## Context

SEooC `unit()` wrappers and underlying runtime libraries are different targets.
Constraints on [the wrappers](../../../BUILD) can reflect build/analysis tooling
rather than implementation support. Inferring platform support from those
constraints would misclassify `ClientConnection` as lacking QNX support.

## Decision

Document implementation/runtime support, build-tooling readiness, and analysis
coverage separately. `ClientConnection` supports QNX; limitations of a SEooC
wrapper do not change that implementation support.

QNX dispatch is the safety-relevant backend; Linux Unix Domain is QM. Evidence
for ASIL-B mechanisms must come from the QNX implementation and relevant tests,
not an analogous Linux path. This scope statement is not a certification claim.

## Consequences

Keep current limitations, revalidation evidence, and follow-up checks in
[the tooling record](../issues/tooling-qnx-unit-analysis.md), not a component-porting
task. Empty wrapper implementation lists do not establish implementation analysis
coverage.

## Evidence

- Explicit project clarification during the 2026-11-04 knowledge-layout review:
  the client implementation supports QNX; tooling support is still developing.
- [Runtime libraries and wrapper constraints](../../../BUILD).
- [Platform aliases](../../../client_factory.h) and
  [platform requirements](../../../dependability/requirements/component_requirements.trlc).