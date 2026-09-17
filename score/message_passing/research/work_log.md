# Message Passing — Work Log

Append-only. Top-level log for shared scratchpad activity (before any `changes/<cycle>/` exists).
Never rewrite history — add new dated entries only.

## 2026-08-31 — Bootstrap: baseline snapshot (Cycle 0)

- Confirmed `score/message_passing/research/` did not exist yet.
- Read, read-only, the existing frozen artifacts under `score/message_passing/dependability/`:
  `assumed_system/` (`aous.trlc`, `assumed_system_requirements.trlc`), `requirements/`
  (`feature_requirements.trlc`, `component_requirements.trlc`,
  `external_component_requirements.trlc`), `software_architectural_design/` (`static_design.puml`,
  `client-server.md`, and the other diagrams by name), `safety_analysis/` (`failure_modes.trlc`,
  `control_measures.trlc`, and the eight `fta_*.puml` diagrams by name), and confirmed
  `software_unit_design/` currently has no content beyond its `BUILD` file. Also read the current
  public headers (`i_client_connection.h`, `i_client_factory.h`, `i_server.h`,
  `i_server_factory.h`, `i_server_connection.h`, `i_connection_handler.h`,
  `service_protocol_config.h`) and confirmed `integrity_level = "B"` / `maturity = "development"`
  from `dependability/BUILD`.
- Wrote `research/problem_statement.md` as a reverse-documented baseline snapshot (terminology,
  system slice, requirement/failure-mode/control-measure index, current maturity/ASIL), explicitly
  marked as orientation material, not source of truth.
- Confirmed `score/mw/com/impl/bindings/lola/messaging/` exists as a known consumer of the
  client/server connection API and recorded it in `research/references.md` as orientation-only
  evidence, per the bootstrap prompt's instruction not to let it drive the baseline content.
- Seeded `research/backlog.md` (placeholder AoUs, incomplete control measures for 7 of 8 failure
  modes, empty `software_unit_design/`, stale/unresolved TODOs in `client-server.md`) and
  `research/nice_to_haves.md` (shared-memory handle passing, watchdog-friendly notification
  callbacks, shared thread pools) from things noticed during the read-through.
- Did not modify anything under `dependability/` or the public headers — read-only reconnaissance
  only. Did not create a `changes/<cycle>/` directory — no concrete change was defined in this
  pass.
- Stopped per the prompt's stop condition to ask the human what the first real actualization cycle
  should be about.

## 2026-09-17 — Backlog: memory/size-limits findings deferred

- The `changes/2026-09-01-client-identity-and-userdata-docs/` cycle (Step 7) is closed; no active
  cycle is open.
- Logged `dependability/software_architectural_design/memory_and_size_limits_findings.md` (written
  2026-09-16, outside any cycle) as a `backlog.md` entry — identifier length-limit
  inconsistency/undocumented SSO sizing preference, missing per-connection memory-usage model, the
  `pre_alloc_connections`/server-side `max_queued_sends` dead-configuration-vs-ASIL-B-requirement
  contradiction, and the missing memory_resource-containment test. Explicitly deferred — not
  actioned this session, per human instruction.
- No new `changes/<cycle>/` directory created. Awaiting human decision on which backlog/nice-to-have
  item (or a new external trigger) becomes the next actualization cycle's Step 0
  `change_request.md`.
