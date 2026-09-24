# Message Passing — Change Request: diagram-reconciliation-54-findings

## Trigger

`research/backlog.md`'s 2026-09-23 entry ("54 validation findings"): fixing
`software_architectural_design/BUILD`'s wiring (moving `private_api.puml` to `internal_api` and the
three `server_client*_sequence.puml` files to `dynamic`, done in
`changes/2026-09-23-public-api-diagram-requirements-review/`) surfaced 54 previously-silenced
`architectural_design` validator findings: 10 `[Naming]`, 7 new `[Interface]` + 2 pre-existing, 29
`[Method]`, 6 `[Coverage]`. Full breakdown in that cycle's `impact_analysis.md`, Finding 5. Human
explicitly requested resuming this specific item now (this session, 2026-09-24).

## Classification

**"Wrong today" (defect), not "was right, world changed".** `static_design.puml`, the three
sequence diagrams, and `private_api.puml` were each authored independently, using three mutually
incompatible naming schemes for what should be the same units/interfaces, and were never
cross-validated because a `BUILD` wiring bug (`static` instead of `internal_api`/`dynamic`)
silently skipped their validation since they were first added. Nothing about the world changed;
the diagrams were always inconsistent with each other and with the real code — the bug fix just
made the pre-existing inconsistency visible.

## Stated scope

Reconcile `static_design.puml`, `private_api.puml`, and the three `server_client*_sequence.puml`
files so the `component_diagrams` validator's 54 findings are resolved (or consciously
reduced/justified), specifically:
- **[Naming] (10):** unify unit/participant aliases across `static_design.puml`
  (`client_connection`, `server_connection`, `dispatch`, `qnx_dispatch`, `unix_domain`) and the
  sequence diagrams (`client_app`, `client_conn`, `os`, `server`, `server_app`, `server_conn`) —
  currently zero overlap.
- **[Interface] (7 new + 2 pre-existing):** bind real interfaces onto `static_design.puml`'s units
  (currently none — no `-(`/`)-` anywhere in the file) so cross-unit sequence-diagram calls have a
  justifying connection; separately, resolve the 2 pre-existing top-level `message_passing_public_api`/
  `os` findings already logged as Finding 4 in the 2026-09-23 cycle (same root cause family, same
  validator run).
- **[Method] (29):** the sequence diagrams call ~29 method names that resolve to no declared
  internal-API method anywhere.
- **[Coverage] (6):** `private_api.puml`'s placeholder interfaces (`Dispatch.QNX.Client/Server`,
  `Dispatch.UnixDomain.Client/Server`, `IConnectionHandler`, `Server.ServerConnection`, with
  invented methods like `Connect`/`SendMessage`/`Dispatch`/`Accept`/`Listen`) are never called by
  name in any sequence diagram, and don't match any real class (`UnixDomainEngine`,
  `QnxDispatchEngine`, etc. — confirmed in the 2026-09-23 cycle's diagram-fidelity check).

Grounding must come from the real `.cpp`/`.h` code paths (`client_connection.cpp/.h`,
`unix_domain/unix_domain_server.cpp`, `qnx_dispatch/*.cpp`, the public headers), not just prose —
per the lesson already learned the hard way in the sibling paused cycle
(`changes/2026-09-17-fta-redo-grounded-in-architecture/work_log.md`: header/prose-level review
missed a materially wrong mechanism that only direct code reading caught).

## Relationship to the paused `2026-09-17-fta-redo-grounded-in-architecture` cycle

That cycle's `next_steps.md` lists this exact reconciliation as its own item 5, deliberately
sequenced **after** its Step 2 (finalizing FTA `ControlMeasure`/`AoU` content) — reasoning given at
the time: "a diagram section referenced by a basic event's `description` should use the diagrams'
FINAL names". Checked before opening this as an independent cycle: **no existing
`safety_analysis/fta_*.puml` or `control_measures.trlc`/`aous.trlc` content currently references
any sequence-diagram, `static_design.puml`, or `private_api.puml` section/name at all** (repo-wide
grep across `safety_analysis/*.puml`, confirmed empty). The forward-looking sequencing concern is
therefore not an active blocker today — this cycle can proceed independently of, and without
resuming, the still-deprioritized FTA cycle. If the FTA cycle resumes later and its content ends up
referencing specific diagram section names, that would need to reference whatever this cycle
leaves behind as final, not the other way around.

This is opened as a **new, separate `changes/` cycle** (not a resumption of
`2026-09-17-fta-redo-grounded-in-architecture`), scoped purely to architecture-diagram content —
no `safety_analysis/`, `requirements/`, or `assumed_system/` files are expected to be touched.

## Open Questions

1. **Alias/naming convergence direction:** should the sequence diagrams be renamed to match
   `static_design.puml`'s existing unit aliases (`client_connection`/`server_connection`/`dispatch`/
   `qnx_dispatch`/`unix_domain`), or should `static_design.puml` be extended with new aliases that
   better match what the sequence diagrams actually depict (e.g. distinguishing the calling
   application from the library unit, which `static_design.puml`'s aliases don't currently do)?
   Default assumption unless told otherwise: converge on `static_design.puml`'s existing aliases
   for library-internal units, and keep `client_app`/`server_app`-style aliases only for the
   external caller side (which `static_design.puml` correctly doesn't model as a unit of the
   SEooC).
2. **`private_api.puml` rewrite scope:** rewrite its placeholder interfaces to match the real
   internal classes/methods found by direct code reading (`UnixDomainEngine`, `QnxDispatchEngine`,
   `ClientConnection`, `UnixDomainServer`/`QnxDispatchServer`'s connection-handling paths, etc.), or
   is a narrower fix acceptable (e.g. only fixing method names, keeping the existing
   class/interface groupings)? Default assumption: full rewrite grounded in real classes, since the
   current placeholders match nothing.
3. **The 2 pre-existing `[Interface]` findings (Finding 4 from 2026-09-23, `static_design.puml`
   top-level `message_passing_public_api`/`os` interfaces having no matching top-level declaration
   in `public_api.puml`):** include in this cycle's fix (same validator run, related root cause —
   `static_design.puml` interface bindings), or keep genuinely separate since it's about
   `public_api.puml` rather than `private_api.puml`/sequence diagrams? Default assumption: include,
   since it is the same class of problem (`static_design.puml` interface-binding completeness) and
   touching that file for the other 52 findings anyway makes it a natural, minimal-additional-cost
   fix — but flag explicitly whether `os` should really be modeled as a top-level SEooC-bound
   interface at all (per that finding's own open question), rather than mechanically patching it.
4. **Depth of code-grounding:** should this cycle re-derive the sequence diagrams' *content*
   (message flow correctness) from the real code, or only fix the *mechanical* validator findings
   (naming/interface/method/coverage alignment) while leaving the current happy-path-only scope of
   the sequence diagrams unchanged? Default assumption: fix mechanical alignment only — expanding
   sequence-diagram *scope* (e.g. adding failure paths) is the substantial, separate work already
   captured in the paused FTA cycle's `change_request.md`/`impact_analysis.md`; conflating the two
   would blow past "minimal diff".

## Checkpoint — RESOLVED 2026-09-24 (human)

Human confirmed the trigger, classification, and stated scope, and accepted the stated defaults
for Open Questions 1, 2, and 4 as-is. Open Question 3's sub-point (should `os` really be modeled
as a top-level SEooC-bound interface?) was answered explicitly, grounded in documentation rather
than by default:

- `bazel/rules/rules_score/docs/user_guide/architectural_design.rst` documents a **named-port**
  pattern specifically for this case — a `portin`/`RequiredInterface` example bound with
  `p_required -( RequiredInterface : requires` — i.e. an external dependency the SEooC *requires*
  (not just interfaces it *provides*) is a first-class, intended part of the static diagram, not
  an afterthought.
  See [architectural_design.rst](/home/qxx6787/.cache/bazel/_bazel_qxx6787/57d6cb68c88af0fdba39f028547e1e7c/external/score_tooling+/bazel/rules/rules_score/docs/user_guide/architectural_design.rst).
- `validation/core/docs/specifications/component_public_api.md`'s "SEooC Relationship Consistency"
  section confirms the validator treats this uniformly: "Any PlantUML relation type ... and any
  endpoint role (required, provided, or none) is accepted as long as the SEooC entity is the
  source and the public API interface is the target" — i.e. the tooling makes no distinction
  between the SEooC *depending on* an environment interface (like the OS) and the SEooC *exposing*
  one; both must be declared top-level and connected from the SEooC to pass validation.

Conclusion: `os` **should** stay modeled as a top-level, SEooC-bound (required) interface in
`static_design.puml` — dropping it to silence the finding would hide a real, documentation-worthy
dependency (message_passing relies on OS syscalls it does not control, which is exactly the kind
of environment relationship this pattern exists to make explicit). The fix is therefore to declare
`os` as a matching top-level interface in `public_api.puml` (with the OS-level operations
message_passing actually depends on — sockets/poll/etc. — to be enumerated during Step 1 impact
analysis from real code), not to remove the relationship. Same reasoning applies to
`message_passing_public_api` (the provided side) — both findings get fixed by extending
`public_api.puml`, not by trimming `static_design.puml`.

All 4 Open Questions are now resolved. Proceeding to Step 1 (impact analysis).
