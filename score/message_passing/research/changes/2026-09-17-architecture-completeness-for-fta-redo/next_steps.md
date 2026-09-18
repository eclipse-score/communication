# Message Passing — Next Steps: architecture-completeness-for-fta-redo

Current step: **Step 7 — evidence bundle complete, cycle closed** (`rules-score-actualize`
lifecycle table).

All three open questions were answered by the human and acted on in this cycle:

1. New dedicated diagram files, not an in-place extension.
2. Scope refined into three categories (contract misuse → AoUs, internal logic, OS/transport
   faults); both non-AoU categories now have grounded sequence diagrams, validated via `bazel
   build`.
3. FTA redo continues automatically as the next cycle — see
   `changes/2026-09-17-fta-redo-grounded-in-architecture/` for its Step 0/1.

No further edits queued for this cycle.
