# Work Log

Append-only. New entries go at the bottom with a date; never edit past entries — corrections get a
new entry.

## 2026-08-24 — Bootstrap (Step 0)

- Started the SEooC lifecycle for `score/service_discovery/` per
  `.github/prompts/bootstrap-service-discovery-seooc.prompt.md` and the `rules-score` skill.
- Created `research/` with this scratchpad structure.
- Copied `Input for new Service Discovery Implementation.docx` into `research/inputs/` and added a
  plain-text/markdown rendering (`inputs/input_for_new_service_discovery_implementation.md`) for
  greppability.
- Read, as read-only evidence (not as a requirements source):
  - `score/mw/com/service_discovery/` PoC — daemon, message-passing client/server adapters,
    binary protocol, in-memory registry with creation/usage locks, UID/PID capture, and its
    (to-be-discarded) `dependability/` TRLC/PlantUML content.
  - `score/mw/com/` integrator — `impl::IServiceDiscovery`/`impl::IServiceDiscoveryClient`
    abstractions, `SkeletonBase`/`ProxyBase` call sites, and the LoLa binding's actual
    flock/inotify-based `ServiceExistanceMarkerFile`/`ServiceUsageMarkerFile` implementation
    (found in `impl/bindings/lola/`, not in `service_discovery/`).
  - `score/message_passing/` — `ServiceProtocolConfig` size-limit semantics
    (`max_send_size`/`max_reply_size`/`max_notify_size`, fixed per protocol kind at setup time,
    `EMSGSIZE` on overflow, no transport-level chunking), `SendWaitReply`/`Notify` semantics.
- Drafted `problem_statement.md` from the above. Key findings folded in as open questions rather
  than resolved: message-passing's fixed-size-per-message-kind constraint has no chunking support,
  so batching/list-style requests need an explicit strategy; whether the LoLa integration glue
  belongs inside this SEooC or purely in `mw/com` is unresolved; ASIL classification is only a
  starting hypothesis (PoC used ASIL.B); whether the legacy marker-file mechanism needs to coexist
  with the daemon during a migration window is unresolved.
- Stopped per the bootstrap prompt's stop condition: awaiting Checkpoint 0 (human confirmation of
  `problem_statement.md`) before touching assumed-system requirements or any TRLC file.
