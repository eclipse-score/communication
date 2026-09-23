# Message Passing — Backlog

Long-lived, shared across all actualization cycles. Opportunistic findings noticed while reading
or working land here (Core Principle 1 of `rules-score-update`) — they are not acted upon until
a future cycle deliberately picks them up as its own change request.

## From the 2026-09-19 assumed-system-requirements-rewrite cycle (closed)

- `Mitigation`/`ControlMeasure` content (what the retired `SafeState` used to gesture at with the
  unverified "safe-silent" term) has no home in `assumed_system_requirements.trlc` anymore and is
  not yet re-authored anywhere. Per the human, it belongs in `safety_analysis/control_measures.trlc`
  once the repo rebases on the `@score_tooling` schema version that actually defines
  `Mitigation`/`ControlMeasure` there (confirmed the vendored
  `third_party/score_requirement_model/score_requirements_model.rsl` copy in this repo does not
  define `Mitigation` at all — the real schema in use must come from a newer external version).
- Reconciling `safety_analysis/failure_modes.trlc`/`control_measures.trlc`/`fta_*.puml` against the
  new assumed-system requirements, and against `research/safety_concept_notes.md`'s 8 principles
  more broadly, remains separate, larger future work — see the paused
  `changes/2026-09-17-fta-redo-grounded-in-architecture` cycle.

## From the 2026-08-31 baseline snapshot (read-only reconnaissance)

- `dependability/assumed_system/aous.trlc` contains no real Assumptions of Use — only a `TODO`
  comment and a single placeholder `ExampleAoU` record (`mitigates = "FailureModeName"`, which
  does not match any real `FailureMode` name in `safety_analysis/failure_modes.trlc`). A future
  cycle should author real AoUs for this SEooC (e.g. host OS guarantees around UID spoofing
  resistance, IPC buffer limits, thread scheduling) and retire the placeholder.
- `dependability/safety_analysis/control_measures.trlc` only defines `ControlMeasure` records for
  one of the eight failure modes/FTAs (`MessageNotDeliveredCorrectly` — `OsIpcFaultHandling`,
  `SendBufferArgumentValidation`, `BE_MessageTooBig`, `BE_SendQueueExhausted`). The remaining seven
  failure modes (`IpcChannelUnavailable`, `NotificationNotDelivered`,
  `ServerMessageHandlingFailure`, `MessageTimingViolated`, `IpcApiMisuseOrLifecycleViolation`,
  `ConnectionContextDataWrong`, `StateMachineError`) each have an `fta_*.puml` diagram but no
  matching control measures in the `.trlc` file yet. Worth a dedicated future cycle.
- `dependability/software_unit_design/` exists (with a `BUILD` file) but is otherwise empty — no
  unit-design content has been authored there yet, despite `component_requirements.trlc` having a
  "Client Unit Requirements (client_connection)" and "Server Unit Requirements" section that would
  naturally feed unit design.
- `client-server.md` explicitly defers several things as "not in scope of the first release":
  passing shared-memory handles over the connection, a paired watchdog-arm/disarm callback
  mechanism for notification timing, and larger shared thread pools for concurrent message
  processing. None of these have corresponding placeholder requirements yet — if any are picked up
  in a future cycle, they will likely need new `FeatReq`/`CompReq` records, not edits of existing
  ones.
- `client-server.md` marks two implementation questions as still-`TODO`: the exact shape of the
  server-side "User Data" object (`void*` vs. `std::uintptr_t` vs.
  `score::cpp::pmr::unique_ptr<IConnectionHandler>`) and the semantics/audience of
  `GetClientIdentity()` for access control ("TODO: TBD"). Worth checking whether the current code
  (`server_types.h`, `i_server_connection.h`) has since resolved these; if so, `client-server.md`
  is stale on this point.

## From the 2026-09-01 client-identity-and-userdata-docs cycle

- `dependability/requirements/external_component_requirements.trlc` is **not wired into any Bazel
  target** — `dependability/requirements/BUILD`'s `component_requirements` target only lists
  `component_requirements.trlc` in `srcs`. `trlc --verify` has never validated this file, and it is
  not part of `dependable_element_message_passing`'s `requirements` list either. Deciding which
  target/component should own it (a new `component_requirements` target of its own? folded into
  the existing one? something else given it represents requirements *towards* the environment
  rather than *of* a component) is a build-structure decision for a dedicated future cycle.
- `TransportMechanismOnLinux` (`external_component_requirements.trlc`) was lowered from
  `safety = ScoreReq.Asil.B` to `ScoreReq.Asil.QM` (`version` 1→2) in this cycle, per explicit
  human confirmation, and no longer derives from `SafetyCertifiedTransportMechanism`. No longer an
  open question.
- The UDS-on-QNX backend's inability to report real client identity (`ClientIdentity{0,0,0}`,
  confirmed in `unix_domain/unix_domain_server.cpp`, `#ifdef __QNX__`) is currently captured only as
  a `note` on the `IServerConnectionGetClientIdentityAPI` `CompReq` and in `client-server.md`
  prose. The human explicitly chose to defer formalizing it as a proper `AoU` wired into the
  `ConnectionContextDataWrong` FTA (`fta_connection_context_data_wrong.puml`) to a future
  safety-analysis-focused cycle — see
  `changes/2026-09-01-client-identity-and-userdata-docs/change_request.md`, Open Question 2.

## From the 2026-09-16 memory/size-limits findings pass (no active cycle)

`dependability/software_architectural_design/memory_and_size_limits_findings.md` was written as a
standalone, read-only findings log (not tied to a `changes/<cycle>/`, and deliberately not yet
TRLC/tests) analyzing `ServiceProtocolConfig`, `IClientFactory::ClientConfig`, and
`IServerFactory::ServerConfig` for missing count/length/memory specifications. Deferred to a future
cycle — not picked up this session. Key points for whoever runs that cycle's impact analysis:

- **No documented/consistent `identifier` length limit**, and the two backends disagree: Unix
  Domain abstract-socket path silently truncates via `memcpy` at ~106 bytes (no error), QNX
  `QnxResourcePath::kMaxIdentifierLen`=256 hard-panics via precondition on violation (and also
  rejects empty identifiers, which Unix Domain accepts). No cross-backend validation exists today.
- **Preferred `identifier` size ≤ 15 bytes** to fit inside libstdc++'s `score::cpp::pmr::string` SSO
  capacity and avoid an extra heap allocation per connection (libc++ SSO is 22 bytes); currently
  undocumented next to the field.
- **No per-connection/per-config memory-usage model** is documented for client (`ClientConnection`)
  or server (`UnixDomainServer`/`QnxDispatchServer`) — rough formulas were derived from reading the
  implementation but not written down as spec.
- **`ServerConfig::pre_alloc_connections` and server-side `max_queued_sends` are dead configuration**
  in both backends today (unused/unread), which directly **contradicts two existing ASIL B
  requirements** in `component_requirements.trlc`:
  `ServerPreallocatesConnectionObjects` and `ServerRingBufferQueueSizeConfigurable`. This should
  likely be resolved (implement to match, or revise the requirements) *before* formalizing any new
  memory-usage-model requirement, since preallocation would change the formulas.
- The per-engine (not per-connection) shared receive buffer sizing is a favorable, currently
  undocumented property worth stating explicitly once this becomes a requirement.
- No test currently proves allocations stay within the caller-supplied `memory_resource` instead of
  silently falling back to the process default; a poisoned-default-resource + counting-resource
  test technique was sketched in the findings doc but not implemented.

## From the 2026-09-17 architecture-completeness-for-fta-redo cycle

- **Candidate AoU content (Category A — user contract misuse), not yet authored as TRLC.** Found
  while grounding the new failure-path sequence diagrams; these belong in
  `assumed_system/aous.trlc` (currently placeholder-only per the 2026-08-31 baseline) rather than
  in a sequence diagram of library-internal or OS-layer behavior:
  - Destructing an `IClientConnection` while not in the `Stopped` state (documented unsafe in
    `i_client_connection.h`'s destructor doc).
  - Starting a connection with `ClientConfig::sync_first_connect = true` from within a callback —
    documented as a deadlock risk (`i_client_factory.h`), matches the existing but never-mitigated
    FTA basic event `BE_SyncConnectDeadlock` in `fta_message_timing_violated.puml`.
  - Calling `IServerConnection::Reply()`/`Notify()`/`RequestDisconnect()` after the corresponding
    disconnection callback has already returned (`client-server.md`, "Server Connection" section).
  - A server's `OnMessageSentWithReply()` (or `MessageCallback` used as the sent-with-reply
    callback) never calling `Reply()` — stalls further sent-with-reply processing on that
    connection; root cause is the server application's own callback, not the library.
  - A `ConnectCallback` returning inconsistent, null, or otherwise invalid `UserData`.
  - Full context and the (deliberately deferred) plan to redo the FTA/control-measures grounded in
    this categorization is in
    `changes/2026-09-17-architecture-completeness-for-fta-redo/evidence_bundle.md`.
- `client_connection_activity_diagram.puml` uses PlantUML *state-diagram* syntax (`state X`,
  `X --> Y`) that the repo's custom `puml_cli` parser (used by the `architectural_design` Bazel
  rule) does not recognize under any of its supported kinds (Component/Activity/Class/Sequence —
  confirmed via direct `bazel build` failure: "Failed to parse ... with any available parser").
  It is therefore **not wired into `software_architectural_design/BUILD`** and has never been
  toolchain-validated. Needs a rewrite into whichever diagram flavor the tool's "Activity" parser
  actually accepts before it can be added. A harmless `skinparam` block→single-line syntax fix was
  applied as a drive-by in the 2026-09-17 cycle, but the deeper state-diagram-syntax issue remains.
- `static_design.puml` has **pre-existing** (not introduced by any recent cycle) non-fatal
  validation warnings surfaced by `bazel build` on the `architectural_design` target: its
  `message_passing_public_api` and `os` interface declarations are not found in `public_api.puml`
  and have no SEooC-boundary relationship ("Public API interface(s) ... have no relationship to
  the SEooC"). Worth a dedicated future cycle to fix `static_design.puml` or `public_api.puml`,
  whichever is actually stale.

## Nice to have vs backlog

Anything that is a *possible future improvement* rather than an *observed inconsistency* goes in
`nice_to_haves.md` instead of here.
