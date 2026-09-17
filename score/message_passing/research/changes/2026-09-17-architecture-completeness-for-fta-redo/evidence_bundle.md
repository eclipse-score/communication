# Message Passing — Evidence Bundle: architecture-completeness-for-fta-redo

## Final change list

1. **New** `software_architectural_design/server_client_os_fault_sequence.puml` — sequence diagram
   for Category C (underlying-layer/OS/transport non-happy paths): connection refused
   (`kPermission`), server bind/startup failure, peer crash/disconnect (`kClosedByPeer`), transport
   I/O error (`kIoError`), `SendWithCallback` reply lost to peer disconnect, `ENOMEM`, server-side
   incoming backpressure delegated to the OS.
2. **New** `software_architectural_design/server_client_internal_fault_sequence.puml` — sequence
   diagram for Category B (library-internal, deterministic, pre-OS-call resource limits):
   message-too-big rejection, client-side async send-queue exhaustion, server-side Notify-queue
   exhaustion, safe failure of re-entrant calls from within a callback.
3. **Modified** `software_architectural_design/BUILD` — added `server_client_sequence.puml` (the
   pre-existing happy-path diagram, previously never wired into the `architectural_design` target)
   and the two new diagrams to the `static` sources list.
4. **Modified** `software_architectural_design/client_connection_activity_diagram.puml` — converted
   `skinparam state { ... }` / `skinparam note { ... }` block syntax to single-line `skinparam`
   statements (drive-by fix enabling this file to at least parse under one of the supported diagram
   kinds later; still not wired into `BUILD`, see below).
5. **Not changed**: `failure_modes.trlc`, `control_measures.trlc`, the 8 `fta_*.puml` diagrams,
   `assumed_system/aous.trlc`, `feature_requirements.trlc`, `component_requirements.trlc`,
   `static_design.puml`, `public_api.puml`, `private_api.puml`.

## Version-bump table

Not applicable — PlantUML architecture-design files in this repo are not `version`-tagged TRLC
records (unlike `FeatReq`/`CompReq`/`FailureMode`/`ControlMeasure`). No downstream `derived_from`
re-pinning was needed since nothing referenced these diagrams by version.

## Ripple map

- `software_architectural_design/BUILD` `architectural_design.static` — updated (item 3 above); no
  other target references these diagram files.
- No `lobster-tracing` ids or `test_case_coverage.lock.yaml` entries reference these diagrams.
- `failure_modes.trlc`/`control_measures.trlc`/`fta_*.puml` are **intentionally left unrippled**
  this cycle — they are the subject of the deferred follow-on cycle, which will consume these new
  diagrams as its grounding evidence.

## Validation

- `bazel build //score/message_passing/dependability/software_architectural_design:message_passing_architectural_design`
  completes successfully. Both new sequence diagrams parse and are packaged as
  `*.fbs.bin` artifacts.
- Pre-existing (not introduced by this cycle) non-fatal validation warnings remain in the build log
  against `static_design.puml`'s public-API interface declarations — logged to `backlog.md`, not
  fixed here (out of scope, unrelated file).

## Residual risk / deferred items (see `research/backlog.md` for full entries)

1. **Category A (user contract misuse) scenarios** were catalogued during this cycle's research but
   deliberately **not** authored as `assumed_system/aous.trlc` records — that file remains
   placeholder-only. Candidate list (destructing a not-yet-`Stopped` `IClientConnection`;
   `sync_first_connect` from within a callback risking deadlock; calling `Reply`/`Notify`/
   `RequestDisconnect` after the disconnection callback returned; `OnMessageSentWithReply` never
   calling `Reply`; a `ConnectCallback` returning inconsistent/null `UserData`) is recorded in
   `backlog.md` for whoever authors real AoUs next.
2. **`client_connection_activity_diagram.puml`** uses PlantUML state-diagram syntax not recognized
   by the repo's custom `puml_cli` parser (tried Component/Activity/Class/Sequence, all failed) —
   it needs a syntax rewrite (likely into the `Activity`-diagram flavor the tool supports) before it
   can be wired into `BUILD` and validated. Not attempted this cycle (distinct task from sequence
   diagrams).
3. **`static_design.puml`** has pre-existing (not introduced here) validation errors: its
   `message_passing_public_api`/`os` interface declarations aren't found in `public_api.puml` and
   have no SEooC-boundary relationship. Logged, not fixed — unrelated to this cycle's scope.
4. Per the human's explicit choice, the **FTA/control-measures redo opens automatically as the next
   cycle** (`changes/2026-09-17-fta-redo-grounded-in-architecture/` or similar), still subject to
   that cycle's own Step 0/1 checkpoints before any `.trlc`/`.puml` safety-analysis content changes,
   given its ASIL B stakes.
