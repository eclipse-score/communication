# Message Passing — Work Log: architecture-completeness-for-fta-redo

## 2026-09-17 — Cycle opened

- Trigger: human stated the current FTA is not meaningful and requirement-linking is pointless;
  before redoing FTA, architectural design (incl. sequence diagrams) must be complete.
- Read all 8 `fta_*.puml` diagrams, `control_measures.trlc`, `failure_modes.trlc`,
  `server_client_sequence.puml`, `client_connection_activity_diagram.puml`, `client-server.md`, and
  `i_client_connection.h` to perform Step 1 impact analysis.
- Wrote `change_request.md` (Step 0) — classified as "wrong today" (completeness defect), not
  "world changed". Scope split into two pieces: this cycle = complete architecture; a deferred
  future cycle = redo FTA/control measures once architecture is grounded.
- Wrote `impact_analysis.md` (Step 1) — root cause traced to
  `software_architectural_design/server_client_sequence.puml` +
  `client_connection_activity_diagram.puml` only documenting the happy path; downward ripple
  confirmed minimal (no lobster-tracing/test-lock references to these diagrams; FTA/control-measure
  files deliberately left untouched this cycle).
- Stopped before editing any `.puml` file to get human checkpoint confirmation on: the impact set
  (Step 1 checkpoint) and the candidate list of 9 failure/error scenarios plus the in-place-vs-new-
  file question (`change_request.md` Open Questions 1–2), per `rules-score-actualize`'s discipline
  of not starting Steps 2+ before Step 1 is confirmed.

## 2026-09-17 — Human answers received; Steps 2–6 executed

- Human chose: new dedicated diagram files (not extended in place); refined the scope into three
  categories (user contract misuse → AoUs, library-internal logic, underlying-layer/OS faults);
  FTA redo continues automatically as the next cycle.
- Read remaining public headers (`i_client_connection.h`, `i_client_factory.h`, `i_server.h`,
  `i_server_connection.h`, `i_server_factory.h`, `i_connection_handler.h`, `server_types.h`) to
  ground every diagram statement in documented behavior (StopReason enum values, Send()/
  SendWaitReply()/SendWithCallback() doc comments, ConnectCallback/UserData signatures) rather than
  invented content.
- Read `software_architectural_design/BUILD` and confirmed `server_client_sequence.puml` and
  `client_connection_activity_diagram.puml` were never wired into the `architectural_design`
  target's `static` sources — never validated by the toolchain.
- Created `server_client_os_fault_sequence.puml` (Category C) and
  `server_client_internal_fault_sequence.puml` (Category B).
- Updated `BUILD` to add `server_client_sequence.puml` plus the two new files to `static`.
- Ran `bazel build //score/message_passing/dependability/software_architectural_design:message_passing_architectural_design`;
  fixed two `puml_cli` parser incompatibilities in the new files (unsupported `...` narrative line;
  unsupported `-x` lost-message arrows) until it built cleanly.
- Attempted to add `client_connection_activity_diagram.puml` too; discovered its PlantUML
  state-diagram syntax isn't recognized by any of the tool's supported diagram kinds. Fixed its
  unrelated `skinparam` block-syntax issue as a drive-by, but left it out of `BUILD` and logged the
  deeper state-diagram-syntax issue to `backlog.md` as a separate deferred task.
- Noted (not fixed) pre-existing, unrelated validation warnings against `static_design.puml`
  surfaced by the same build run.
- Wrote `evidence_bundle.md` (Step 7). Cycle closed.
