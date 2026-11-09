# Requirements and Allocation

Read when editing TRLC requirements, their level, version pins, or allocation.
Scope: message-passing SEooC; tooling 3.0.2, inspected on 2026-11-04.

## Author Intent Before Wiring

Use the [requirements skill](../../../../.github/skills/score-requirements/SKILL.md)
for the general workflow. Agree ambiguous content, level, and ASIL with the
requirements reviewer before transcribing it. Resolve schema questions against
the pinned model described in [tooling](tooling.md), not an obsolete example.

| Level | Current source | Allocation |
| --- | --- | --- |
| Assumed system | [AssumedSystemReq records](../../dependability/assumed_system/assumed_system_requirements.trlc) | SEooC requirements |
| Feature | [FeatReq records](../../dependability/requirements/feature_requirements.trlc) | SEooC requirements |
| Component | [CompReq records](../../dependability/requirements/component_requirements.trlc) | `component_message_passing.requirements` |
| Environmental obligation | Own AoU, once agreed and wired | Safety-analysis measures and forwarding |

[The requirements BUILD](../../dependability/requirements/BUILD) resolves
parents through `deps`; [dependability/BUILD](../../dependability/BUILD) allocates
the targets. A file being present, or a reference resolving through `deps`, does
not itself establish allocation or test coverage.

## Component-Specific Authoring Lessons

Assumed-system records express externally useful capabilities: can an integrator
selecting an IPC library determine whether the system offers the needed model,
interaction, failure detection, or identity information? Do not push detailed
failure-handling algorithms into that level.

Current `PlatformAbstraction` is QM; `QnxAsilBQualifiedImplementation` captures
the QNX safety-target commitment. Do not equate an OS-independent API contract
with identical qualification or implementation properties on every OS.
`ClientInterface` and `ServerInterface` feature records anchor the existence of
those interfaces; `OSIndependentAPI` addresses a different characteristic.

Use bounded/configured one-way messaging terminology rather than implying that
"send without reply" means unlimited buffering or no failure result.

## Subject and Responsibility

If the subject is behavior implemented and verifiable inside message_passing,
use a component requirement. An obligation imposed on the OS or integrator is
an AoU candidate, not an internally implemented safeguard. An own AoU has no
`derived_from`. For wiring and the distinction between export/forwarding and
assessment coverage, see [own AoUs in the safety model](safety-analysis.md#current-traceability-model).

Do not introduce a separate "external requirements" rule for parked candidates.
For identity behavior, candidate obligations, and the wiring status of the
[external requirement file](../../dependability/requirements/external_component_requirements.trlc),
see [identity responsibilities](issues/requirements-client-identity-aous.md).

## Versions and Pins

The current model requires versioned parent references and prevents a derived
FeatReq/CompReq from lowering its parent's ASIL (`QM < B < D`). A `CompReq`
requires at least one parent from FeatReq, AssumedSystemReq, or a received AoU;
its own AoUs and FTA root causes are not valid parents.

For a semantic change, identify changed records, bump their versions, review the
downstream impact, and update reviewed pins. A resolved reference is not proof
that the child remains a sufficient refinement.

Earlier reviews distinguished a same-parent version refresh from a change of
parent identity. The recovered convention and its review status are preserved
in [the versioning proposal](decisions/2026-11-04-requirement-versioning.md).
Do not silently expand that convention into automatic pin updates, or assume
it overrides a later agreed version policy.

## Verification

```sh
bazel test //score/message_passing/dependability/assumed_system/... //score/message_passing/dependability/requirements/...
```

Run from the repository root. This checks wired TRLC inputs, not the parked
external candidate, requirement sufficiency, complete implementation, or full
test tracing. Inspect [build and test](build-and-test.md) for explicit manual
SEooC checks and generated-report freshness. The optional AI-quality check is
not a deterministic conformance proof.

Related findings cover
[safety-analysis completeness](issues/safety-analysis-completeness.md),
[server resource configuration](issues/functionality-server-resource-configuration.md),
and [identifier limits](issues/functionality-identifier-limits.md).