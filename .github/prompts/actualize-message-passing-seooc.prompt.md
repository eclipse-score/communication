---
description: "Start using the rules-score-actualize lifecycle on score/message_passing/: create its research/ scratchpad with a reverse-documented baseline snapshot of the existing (trusted) dependability artifacts, then stop and ask the human what the first concrete actualization cycle should be about."
agent: "agent"
argument-hint: "Bootstrap the message_passing actualization scratchpad; the concrete change comes later"
---

# Bootstrap: Message Passing Actualization Scratchpad (Cycle 0 — Baseline Snapshot)

You are starting to use the **`rules-score-actualize`** skill
(`.github/skills/rules-score-actualize/SKILL.md`) on `score/message_passing/`. Read that skill
first — it defines the impact-analysis-first update process, the `research/` scratchpad layout for
repeated cycles, and the checkpoint discipline you must follow. This prompt only supplies the
component-specific starting facts; do not rely on any other session or host-specific memory.

## Why this component, and why `rules-score-actualize` and not `rules-score`

`score/message_passing/` already has a real `dependable_element` at
`score/message_passing/dependability/` (`integrity_level = "B"`, `maturity = "development"`) with
frozen `assumed_system/`, `requirements/`, `software_architectural_design/`, and
`safety_analysis/` content that passes verification today. This is **not** a discardable PoC —
treat it as the trusted baseline. Any future change to it must go through
`rules-score-actualize`'s impact-analysis-and-cascade process, never through `rules-score`'s
"re-derive fresh, discard old names" rework path.

## What you are doing in this prompt

Only the **first bullet of `rules-score-actualize`'s "If `research/` does not exist yet for this
component"** section — nothing else. Concretely:

1. Confirm `score/message_passing/research/` does not already exist. If it does, stop and ask the
   human how to proceed instead of overwriting it.
2. Read, read-only, the existing frozen artifacts under `score/message_passing/dependability/`:
   `assumed_system/` (`aous.trlc`, `assumed_system_requirements.trlc`), `requirements/`
   (`feature_requirements.trlc`, `component_requirements.trlc`, `external_component_requirements.trlc`),
   `software_architectural_design/` (`static_design.puml`, `public_api.puml`, `private_api.puml`,
   `client_connection_activity_diagram.puml`, `server_client_sequence.puml`, `client-server.md`),
   and `safety_analysis/` (`failure_modes.trlc`, `control_measures.trlc`, `fta_*.puml`). Also skim
   the public headers in `score/message_passing/` itself (`i_client_connection.h`,
   `i_client_factory.h`, `i_server.h`, `i_server_factory.h`, `i_server_connection.h`,
   `i_connection_handler.h`, `service_protocol_config.h`) for the current public API surface these
   artifacts describe.
3. Write `score/message_passing/research/problem_statement.md` as a **reverse-documented baseline
   snapshot** — per the skill's template guidance: terminology already in use (e.g. what this
   component calls a "protocol", a "session", a client vs. server connection), the system slice
   (same-host IPC abstraction, Linux Unix-domain-socket and QNX native-dispatch backends behind one
   API), an index of the existing `AssumedSystemReq`/`FeatReq`/`CompReq`/`FailureMode`/
   `ControlMeasure` records (names + one-line gist each, not full text), and the current
   `integrity_level`/`maturity`. State explicitly, near the top of the file, that this is
   orientation material reverse-documented from a trusted baseline, not a fresh Step-0-style
   derivation, and that the frozen TRLC/PlantUML remain the authoritative source of truth.
4. Note **`score/mw/com/impl/bindings/lola/messaging/`** in `research/references.md` as one known,
   concrete, non-exclusive consumer of `score/message_passing/` — it uses the client/server
   connection API for LoLa method calls (see `message_passing_service.{h,cpp}`,
   `message_passing_client_cache.{h,cpp}`). Record this only as orientation evidence for future
   impact analyses ("does a change affect this consumer's usage pattern?") — `message_passing`'s
   public API and architecture must stay consumer-agnostic; do not let this one consumer's needs
   drive the baseline snapshot's content, and do not go looking for or assume any other specific
   consumer beyond what already exists in the repository today.
5. Seed `research/backlog.md` and `research/nice_to_haves.md` with anything you noticed while
   reading (e.g. inconsistencies between the code and the frozen diagrams/requirements, TODOs in
   the design docs) — capture only, do not fix anything yet.
6. Write a first dated entry in a top-level `research/work_log.md` describing this bootstrap (there
   is no `changes/<cycle>/` directory yet — that only gets created once a concrete change is
   defined, per the skill).

## What NOT to do in this prompt

- Do not modify anything under `score/message_passing/dependability/` or
  `score/message_passing/*.{h,cpp}` — this is a read-only reconnaissance pass.
- Do not invent a concrete change, defect, or new requirement to act on. There is no specific
  trigger yet — do not manufacture one just to have something to do.
- Do not create a `research/changes/<cycle>/` directory yet.

## Stop condition

After writing `problem_statement.md` and the shared scratchpad files, stop. Summarize the baseline
snapshot for the human and explicitly ask what the first real actualization cycle should be about
(a new need, a known defect, an assumption that no longer holds, or something else) — per
`rules-score-actualize` Step 0, that answer becomes `research/changes/<first-cycle-slug>/
change_request.md` in a follow-up run, not in this one.
