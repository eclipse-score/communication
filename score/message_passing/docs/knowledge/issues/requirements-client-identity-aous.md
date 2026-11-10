<!-- ----------------------------------------------------------------------------
  Copyright (c) 2026 Contributors to the Eclipse Foundation

  See the NOTICE file(s) distributed with this work for additional
  information regarding copyright ownership.

  This program and the accompanying materials are made available under the
  terms of the Apache License Version 2.0 which is available at
  https://www.apache.org/licenses/LICENSE-2.0

  SPDX-License-Identifier: Apache-2.0
----------------------------------------------------------------------------- -->

# Separate Identity Behavior from Environmental Obligations

Status: deferred
Area: requirements
Scope: connection identity and the planned safety-analysis rewrite
Owner: requirements and safety reviewers; assignment pending
Evidence checked: 2026-11-04; source inspection and prior project clarification

## Current State

`OSProvidedClientIdentityPerConnection` in
[component requirements](../../../dependability/requirements/component_requirements.trlc)
states message-passing behavior. That CompReq remains distinct from conditions
the integrating system must satisfy. The `IServerConnectionGetClientIdentityAPI`
note and [client/server design](../../client-server.md) already explain PID
reuse and using a dedicated UID to identify a particular client process.

[The external candidate](../../../dependability/requirements/external_component_requirements.trlc)
still declares `ConnectionIdentityIntegrityGuaranteed` as a CompReq, explicitly
marked "Not built: AoU candidate". It is absent from
[requirements BUILD inputs](../../../dependability/requirements/BUILD). It is
neither a validated environmental obligation nor a forwarded own AoU.

`ClientIdentityIntegrityCheck` is a leaf in
[the identity FTA](../../../dependability/safety_analysis/fta_connection_context_data_wrong.puml),
but there is no completed assessment argument wiring this candidate to it.

## Questions to Preserve

- Which OS guarantees make reported identity trustworthy, and what exact
  obligation belongs in an OS-integrity AoU?
- Under what access-control policy must the integrator dedicate a UID, while
  accounting for PID reuse? Do not impose that configuration on every use case.
- How should the documented testing-only Unix Domain-on-QNX zero identity be
  scoped without treating it as the safety-certified QNX transport?
- Which CompReq and AoUs jointly close the root cause, and how is their
  sufficiency justified at the applicable ASIL?

## Next Check

The prior project decision was to defer AoU creation to the safety-analysis
rewrite, not to drop these obligations or convert all identity behavior to AoUs.
When that work is explicitly scheduled, agree the split and assess the measures
with the current `RootCauseAssessment` model. A candidate measure set is the
identity-reporting CompReq plus the applicable OS-trust and UID-configuration
AoUs; it is not an already accepted assessment.

Check [own-AoU wiring and assessment coverage](../safety-analysis.md#current-traceability-model)
before exporting or assessing candidates; an `assumptions_of_use` target alone
is not proof of either.