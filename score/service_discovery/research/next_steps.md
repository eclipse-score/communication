# Next Steps

Living TODO — reflects the *current* lifecycle step only; stale entries are removed, not
accumulated.

## Current step: Step 0 — Problem Statement (awaiting Checkpoint 0)

`problem_statement.md` has been drafted from:
- `research/inputs/input_for_new_service_discovery_implementation.md` (informal customer input)
- Read-only evidence from `score/mw/com/service_discovery/` (PoC), `score/mw/com/` (integrator),
  and `score/message_passing/` (transport dependency)

**Immediate next action:** get explicit human confirmation that the problem statement's
terminology, system slice, and informal functional requirements are correct and complete enough to
proceed. Specifically confirm/resolve, or explicitly defer, the Open Questions listed in
`problem_statement.md`:

1. Batch message sizing strategy (splitting vs. bounded batch size vs. hybrid).
2. Transparent vs. explicit batching at the LoLa API level.
3. Whether LoLa-specific integration glue lives inside `score/service_discovery/` or in
   `score/mw/com/` only.
4. ASIL classification (starting hypothesis: ASIL.B, per PoC — not yet confirmed).
5. Whether the legacy flock/inotify marker-file mechanism must coexist with the new daemon during a
   migration period.
6. Whether lease-based staleness supervision is needed in addition to connection-loss cleanup.
7. Authorization policy format/source (how the static UID policy reaches the daemon at startup).

**Do not proceed to Step 1** (`AssumedSystemReq`/`AoU` authoring, per `score-requirements`) until
the human has explicitly signed off on Checkpoint 0, per the bootstrap prompt's stop condition.
