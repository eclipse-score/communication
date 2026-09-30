# Historical research input

The local workflow now uses current knowledge under `../maintenance/` and feature
work under `../work/<feature>/`. This directory is retained historical input.
Its old prompts, task lists, status claims, and path conventions are not current
instructions. Do not load all of it or resume work merely because it appears here.

Read a specific record when its provenance or investigation detail is needed, and
check it against current sources. Existing files are preserved for review and later
migration; this marker does not authorize deleting them.

Before any later cleanup, resolve or preserve the acceptance gaps in the legacy
records: final acceptance is pending for `2026-09-22-feature-req-notify-split-and-crossplatform-qm`
and `2026-09-24-client-interface-feature-requirement`; separate final acceptance is
not explicit for `2026-09-01-client-identity-and-userdata-docs` and
`2026-09-17-architecture-completeness-for-fta-redo`. These directories are under
`changes/`. Retiring the workflow-design records does not accept or resume those
cycles. The paused safety work has its own [current handoff](../work/safety-analysis-rework/README.md).

The retired workflow review and migration/validation evidence are recoverable at
commit `a14601bdc398f3cba4f5ec469c306ae425b76d31`, paths
`docs/engineering/rules-score-update-review.md` and
`docs/engineering/work/dependability-workflow/cycles/01-workflow-and-depots.md`.
Use `git show <revision>:<path>` when needed; these are historical records, not
current workflow instructions.
