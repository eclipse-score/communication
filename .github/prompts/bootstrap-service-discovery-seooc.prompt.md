---
description: "Bootstrap the score/service_discovery/ SEooC: create its research/ directory and author an initial problem statement from source material, then stop for human review before any TRLC/architecture work begins."
agent: "agent"
argument-hint: "Start the service_discovery SEooC rework from scratch"
---

# Bootstrap: Service Discovery SEooC (Step 0 — Problem Statement)

You are starting **Step 0** of the SEooC lifecycle described in
[`rules-score`](../skills/rules-score/SKILL.md). Read that skill first — it defines the
process, the `research/` directory convention, and the checkpoint discipline you must follow.
This prompt only supplies the component-specific starting facts; do not rely on any other session
or host-specific memory.

## What you are building

A new, standalone SEooC at `score/service_discovery/` (**not** under `score/mw/com/`). It replaces
a proof-of-concept whose code works but whose requirements/architecture/safety artifacts are
semantically wrong and must be re-derived, not copied.

Do **not** create TRLC, PlantUML, or Bazel artifacts yet, and do **not** modify any existing
directory. Your only job in this prompt is: read the sources below, then create and populate
`score/service_discovery/research/` per the `rules-score` skill, ending with a
`problem_statement.md` draft — then stop and ask the human to confirm it (Checkpoint 0).

## Sources to read (read-only for this task)

1. **PoC implementation** — `score/mw/com/service_discovery/` — the current service-discovery
   daemon/client code and its `dependability/` artifacts. Its dependability part passes TRLC
   verification but is **semantically wrong**; use it only as evidence of what already works
   end-to-end (e.g. protocol shape, daemon lifecycle), never as requirement wording to reuse.
2. **Main current integrator** — `score/mw/com/` (the parent directory the PoC was extracted
   from) — shows how `mw/com` currently wires against the daemon/client and what glue code exists
   there today. Its own `dependability/` is work-in-progress and may also be partially wrong.
3. **Upstream dependency** — `score/message_passing/` — the transport the daemon/client are built
   on (Linux Unix-domain sockets, QNX dispatch). Its `dependability/` is also work-in-progress.
4. **Informal requirements source** — `Input for new Service Discovery Implementation.docx` at the
   workspace root (outside this repository checkout). This is the primary customer-intent input
   for the new component. Copy it, plus a plain-text/markdown rendering of it, into
   `score/service_discovery/research/inputs/` so its content is greppable/diffable in git.
5. Process references already used for this repo:
   `.github/skills/score-requirements/SKILL.md`, `.github/skills/score-architecture/SKILL.md`,
   `.github/skills/score-safety-analysis/SKILL.md`, `.github/skills/score-testing/SKILL.md`.

## Known constraints to carry into the problem statement

- Primary target OS is QNX 8; most code integration tests run on Linux, and **all** process-artifact
  (non-code) integration tests run on Linux only.
- The service-discovery ⇄ daemon protocol must ride the existing `score/message_passing/` transport
  abstraction — this is a given constraint from the informal input, not a design choice to
  reconsider here. Both RPC-shaped exchanges (`SendWaitReply`) and asynchronous notifications are
  available and neither is mandated over the other; choose per interaction based on its semantics.
  The real constraint is that `score/message_passing/` requires a maximum message size to be fixed
  in advance per protocol message kind: it must be large enough for any single atomically
  meaningful request, but a group request (e.g. subscribing to a list of services in one call) may
  not fit in one protocol message and needs an explicit strategy (e.g. splitting into multiple
  messages, or a bounded batch size) — capture this as an open question/requirement seed, not as
  something silently sized by guesswork.
- The informal input also describes two mechanisms the daemon should absorb that today live outside
  service discovery (flock-based `ServiceExistanceMarkerFile` / `ServiceUsageMarkerFile`): treat
  these as in-scope functional requirements to capture in the problem statement, not as a separate
  concern.
- Whether the `mw/com` integration wrapper code should move out of the new
  `score/service_discovery/` component (since a SEooC out of context arguably shouldn't contain
  `mw/com`-specific glue) is a real open question — record it in `research/backlog.md` or
  `research/nice_to_haves.md`, do not resolve it by moving code now.
- Target maturity for this cycle is `"development"`. Safety classification (ASIL) is not yet
  confirmed — the PoC currently uses `ASIL.B` throughout; treat that as a starting hypothesis to
  confirm at Step 1 (assumed-system requirements), not as settled fact.

## What to produce in this prompt's scope

Create, under `score/service_discovery/research/` (see the `rules-score` skill for the full
directory contract and the `problem_statement.md` template):

- `inputs/` with the copied `.docx` and its text rendering.
- `problem_statement.md` — drafted from the sources above, **not** dictated by this prompt. Derive
  terminology, the system slice, and the informal requirements yourself from the docx and the code
  layout; do not paraphrase this prompt back as if it were the problem statement.
- `work_log.md` with a first dated entry describing this bootstrap.
- `next_steps.md` pointing at Checkpoint 0 (human confirmation of the problem statement) as the
  immediate next action.
- `references.md`, `backlog.md`, `nice_to_haves.md` — start these now, even if only seeded with the
  open questions already surfaced above; keep them current as the `rules-score` skill describes.

## Stop condition

After drafting `problem_statement.md`, stop. Summarize it for the human and explicitly ask for
Checkpoint 0 confirmation before touching assumed-system requirements or any TRLC file. Do not
proceed to Step 1 in this same run unless the human explicitly says to continue.
