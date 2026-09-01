# Message Passing — Backlog

Long-lived, shared across all actualization cycles. Opportunistic findings noticed while reading
or working land here (Core Principle 1 of `rules-score-actualize`) — they are not acted upon until
a future cycle deliberately picks them up as its own change request.

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

## Nice to have vs backlog

Anything that is a *possible future improvement* rather than an *observed inconsistency* goes in
`nice_to_haves.md` instead of here.
