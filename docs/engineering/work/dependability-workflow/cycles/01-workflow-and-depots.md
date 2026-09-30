# Workflow and initial depots

Status: ready for review
Baseline: `7f492b81c8e24846b4c3a90e8bc1f792c6fb6e86`; Linux host;
Bazel 8.7.0, score_tooling 2.2.2.
Request: write the agreed skill, initial depots, and a human-facing workflow/rationale
document that agents must not routinely modify.

## Decisions and authorization

The human agreed to three knowledge scopes and temporary feature directories, then
clarified parallel-branch contention, retained human-gated cycles, and independent
feature retirement. The human authorized writing this package. Artifact acceptance
has not been given.

Upstream tooling and copied `score-*` skills are outside local ownership.
Development warnings are permitted; generated docs expose relevant misalignment.
QNX functional tests are available through `--config=qnx` and relate to dependability
through their exercised behavior and traces.

## Impact

- `rules-score`: shared knowledge/cycle workflow and bootstrap mode.
- `rules-score-update`: incremental impact and cascade mechanics.
- Shared references, component routing prompt, and local tiered-FTA pointers:
  align with current depot/work locations.
- External, repository, and subsystem depots: current facts and references.
- Paused safety work: preserve a current feature/cycle handoff without resuming it.
- Human rationale: separate from routine context and protected by scoped guidance.
- Product artifacts, upstream copies, and legacy research content: unchanged.

## Research evidence

Commands below were run against the baseline before workflow-file editing.

| Check | Result | Interpretation |
|---|---|---|
| `bazel mod show_repo score_tooling` | Resolved 2.2.2 from the pinned archive | Selected tooling source inspected directly |
| `bazel test //score/message_passing/...` | 11 passed, QNX dispatch skipped | Host functional/schema checks; manual targets not implied |
| Explicit component and dispatch tests | Failed; 57 and 37 Lobster diagnostics | Standalone reports lack feature inputs; further tracing gaps remain |
| Explicit safety-analysis test | Failed; 91 Lobster diagnostics | Unresolved interface/FTA trace links, not a parser-unavailable result |
| Explicit dependable-element test | Passed with Lobster warning | Development policy permits findings, including stale pin and missing test traces |
| `bazel build //docs/sphinx:sphinx_doc` | Succeeded | Generated component-requirements report exposes expected-2/current-3 mismatch |
| QNX build/runtime | Not run | Configuration and human command guidance retained; no runtime claim |

The resolved schema permits direct component-to-assumed-system derivation.
Validator source confirms outer-parenthesized labels skip method-name checks.
Other historical parser limitations remain attributed reports, not new reproductions.

## Migration evidence and unresolved history

The earlier review at `b8bbca3218f171d07e6b7577e4005bea26aa1479` covers eight
legacy cycles. Their records remain recoverable under
`score/message_passing/research/changes/`:

| Cycle | Recorded state to preserve |
|---|---|
| 2026-09-01-client-identity-and-userdata-docs | Reported closed; separate final acceptance not explicit |
| 2026-09-17-architecture-completeness-for-fta-redo | Reported closed; separate final acceptance not explicit |
| 2026-09-17-fta-redo-grounded-in-architecture | Paused; current handoff extracted into the safety-analysis feature |
| 2026-09-19-assumed-system-requirements-rewrite | Explicit acceptance recorded |
| 2026-09-22-feature-req-notify-split-and-crossplatform-qm | Final acceptance pending in the records |
| 2026-09-23-public-api-diagram-requirements-review | Explicit acceptance recorded |
| 2026-09-24-client-interface-feature-requirement | Final acceptance pending despite closed wording |
| 2026-09-24-diagram-reconciliation-54-findings | Explicit acceptance recorded, with validator exclusions |

These are provenance statements, not new acceptance or instructions to resume.
No additional features were invented for old acceptance gaps. If such work is
selected, recover its record and establish a current cycle. Before later deleting
legacy research, preserve these dispositions and all needed evidence.

## Package validation

- All 113 local Markdown links in the 21 added/modified files resolved.
- The three local skills passed the bundled skill validator on temporary copies
  omitting only the retained VS Code `argument-hint` field. The unmodified files
  are rejected by that validator for the unsupported field; no native pass is claimed.
- YAML/frontmatter was parsed, and description lengths are 290, 306, and 317
  characters for rules-score, rules-score-update, and tiered-FTA respectively.
- `git diff --check` passed. Scope review found no product-code, TRLC, PlantUML,
  BUILD, or copied upstream `score-*` skill changes and no research-file deletion.
- Author walkthrough covered orientation without a trigger, review-only requests,
  paused FTA resumption, incremental and bootstrap routing, cycles across sessions,
  staged knowledge meeting newer shared facts, independent feature retirement,
  and the human rationale edit restriction. This is an instruction review, not
  an independent-agent behavior test or a VS Code execution trial.
- No additional functional test run was needed for this documentation/skill change.
  The earlier Bazel/Sphinx results above establish baseline research evidence.

## Handoff

Present the authored files for human review. Retain
legacy research and the original review draft as historical material; removal is
separate work. The safety feature remains paused.

Acceptance: not given.
