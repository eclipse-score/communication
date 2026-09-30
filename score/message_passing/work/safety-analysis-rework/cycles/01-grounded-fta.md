# Ground the FTA redo

Status: paused
Baseline: original cycle began 2026-09-17; current handoff reconciled against
`7f492b81c8e24846b4c3a90e8bc1f792c6fb6e86`.
Scope: content review of eight failure modes/FTAs and justified controls/AoUs.
Decision gate: explicit human resumption and review of revised dispositions before
dependent product edits. This migration supplies a handoff, not resumption.

## Current conclusions

The human explicitly deprioritized this work on 2026-09-24. Its proposed answers are
not accepted TRLC content. Use the current [safety intent](../../../maintenance/decisions.md)
and [open items](../../../maintenance/open-items.md), particularly MP-03 through MP-06.

The prior implementation pass relied too much on Unix-domain behavior. Re-ground
ASIL-B claims directly in shared and QNX-dispatch code. Partial mitigation can coexist
with an AoU; no uniform containment or timing guarantee has been established.

## Decisions still needed

| Topic | Current investigation boundary |
|---|---|
| Timing supervision | Decide the integrator obligation and actual partial mitigation; no built-in watchdog guarantee is established. |
| Duplicate disconnect causes | Resolve QNX's no-op `RequestDisconnect` before consolidating obligations. |
| Server queue configuration | The configuration is unused; consider event retirement together with the contract/implementation discrepancy. |
| Health check and retry | Re-derive the actual retry/error-classification mechanism rather than assuming two controls from their names. |
| Missing notification callback | Scope any obligation to a protocol that expects notifications. |
| Handler failure or missing reply | Distinguish one-way delivery from request/reply before merging obligations. |
| Call-flow and lifecycle misuse | Separate detected misuse from residual caller obligations; inspect server lifecycle too. `Restart` is void, not an `EINVAL` result. |
| Invalid connect-callback data | Verify actual value/handler alternatives and failure behavior before consolidating causes. |
| Absent required callbacks | Inspect the selected callback dependency's empty-invocation behavior; graceful no-op is unverified. |
| Notification queue exhaustion | Ground the analysis in the QNX pool/transport. UDS has kernel buffering even without an analogous library-managed queue. |

## Investigation history and evidence

- The original cycle proposed reviewing all eight failure modes and reconciling
  basic events with controls/AoUs; several initial dispositions were revised.
- Human review required implementation grounding instead of header-derived claims.
- The 2026-09-24 resumption investigated code, then human review corrected the
  platform emphasis and explicitly re-paused the work.
- Workflow migration preserved these questions without performing safety edits.
  The old `SafeState` rewrite and diagram-reconciliation tasks are superseded:
  `SafeState` was removed and diagram reconciliation happened separately. Neither
  is a reason to recreate the old task sequence.

Original records are recoverable at
`b8bbca3218f171d07e6b7577e4005bea26aa1479` under
`score/message_passing/research/changes/2026-09-17-fta-redo-grounded-in-architecture/`
using `git show <revision>:<path>`. Read the change request, impact analysis, and
latest next-steps correction only when their detail is needed. Their proposals and
old status claims require reconciliation, not blind replay.

Validation of a revised FTA: not run; no revised FTA has been authored.

## Handoff

Remain paused until the human explicitly selects this work. On resumption, reread
current requirements/architecture and the relevant QNX/shared implementation,
reconstruct the impact, and present revised dispositions for decision. Do not use
the historical thirteen questions as preapproved changes.

Acceptance: not given.
