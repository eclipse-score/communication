# Message Passing — Work Log: diagram-reconciliation-54-findings

## 2026-09-24 — Cycle opened (Step 0)

- Trigger: `research/backlog.md`'s 2026-09-23 "54 validation findings" entry, per explicit human
  request this session to continue with it.
- Read `changes/2026-09-23-public-api-diagram-requirements-review/impact_analysis.md` (Finding 5,
  full 54-finding breakdown) and `changes/2026-09-17-fta-redo-grounded-in-architecture/next_steps.md`
  (item 5, where the fix was queued) to scope this correctly.
- Confirmed via repo-wide grep of `safety_analysis/*.puml` that no FTA content today references any
  sequence-diagram/`static_design.puml`/`private_api.puml` section by name — the FTA cycle's
  "sequence after FTA content" concern is forward-looking, not an active blocker, so this is opened
  as its own independent cycle rather than a resumption of that (still explicitly deprioritized)
  cycle.
- Wrote `change_request.md` with 4 open questions (alias-convergence direction, `private_api.puml`
  rewrite scope, whether to fold in the 2 pre-existing `[Interface]` findings, and whether to
  expand sequence-diagram content or fix mechanical alignment only). Awaiting human checkpoint
  before Step 1 impact analysis.

## 2026-09-24 — Step 0 checkpoint resolved

- Human confirmed trigger/classification/scope and accepted defaults for Open Questions 1, 2, 4.
- Open Question 3's sub-point (should `os` be a top-level SEooC-bound interface at all?) was
  answered from documentation, not by default: `architectural_design.rst` documents a
  `portin`/`RequiredInterface` pattern precisely for SEooC dependencies on the environment, and
  `component_public_api.md`'s SEooC Relationship Consistency check treats required and provided
  top-level interfaces identically. Conclusion: keep `os` modeled, fix by extending
  `public_api.puml` to declare it (and `message_passing_public_api`) as matching top-level
  interfaces, not by removing the relationship from `static_design.puml`.
- All 4 Open Questions resolved. Starting Step 1 (impact analysis).

## 2026-09-24 — Steps 1-6 (impact analysis through validation)

- Read all 5 architecture diagram files + relevant real headers/sources (`i_shared_resource_engine.h`,
  `client_connection.h`, `unix_domain_server.h`, `qnx_dispatch_server.h`, `i_client_factory.h`,
  `i_server_factory.h`, `i_server.h`, BUILD files) to ground the rewrite in real code.
- Wrote `impact_analysis.md`, then rewrote `static_design.puml`, `public_api.puml`,
  `private_api.puml`, and all three sequence diagrams.
- Human paused mid-iteration (54→22 findings) with two corrections: (a) `ISharedResourceEngine`
  is `ClientConnection`-only, not shared with server-side units — fixed by removing the
  `server_connection` binding and converting cross-unit calls to `note over` annotations; (b) `os`
  should be grounded in real `score::os::*` classes (QNX dispatch is the primary/ASIL-B reason for
  it), not mirror `ISharedResourceEngine`'s methods, and threading is deliberately excluded for
  brevity. Also handled a drive-by: removed unused `listener_command_`/`listener_endpoint_`
  members from `QnxDispatchServer` (both `eclipse-score-communication` and `copybara-export`
  copies).
- Hit and root-caused two real toolchain bugs during iteration (a `puml_cli` resolver false
  positive on multi-line notes co-occurring with nested-alt notes; a `sequence_internal_api`
  behavior gap where `ExternalEndpoint` isn't actually exempted from Method-Name Consistency
  despite the spec's documented exemption) — see `evidence_bundle.md` and
  `/memories/repo/score-documentation-conventions.md` for full detail and the adopted workarounds.
- Iterated `bazel build` from 54 → 22 → 17 → 9 → 4 → 1 → **0 findings**. Confirmed
  `bazel build //score/message_passing/dependability:dependable_element_message_passing` (full
  component, docs, lobster) and `bazel test //score/message_passing:unit_tests` (6/6) both clean.
- Wrote `evidence_bundle.md` (Step 7). Cycle ready for human acceptance.

## 2026-09-24 — Cycle closed

- Human accepted the evidence bundle. Re-confirmed `bazel build
  //score/message_passing/dependability/software_architectural_design:message_passing_architectural_design`
  still reports 0 findings (cache-hit rebuild) immediately before closing. `research/backlog.md`'s
  2026-09-23 entry marked `RESOLVED 2026-09-24` pointing at this cycle. No further action planned.
