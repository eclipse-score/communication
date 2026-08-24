# References

Useful pointers discovered while researching Step 0. Read-only evidence, not requirements source.

## Informal input
- `research/inputs/Input for new Service Discovery Implementation.docx` — original.
- `research/inputs/input_for_new_service_discovery_implementation.md` — plain-text rendering.

## PoC: `score/mw/com/service_discovery/`
- `service_discovery_daemon.{h,cpp}` — request handler: Register/Unregister/Resolve/
  StartFindService/StopFindService/creation & usage lock acquire-release.
- `service_discovery_message_passing_server.{h,cpp}` — daemon-side message_passing adapter;
  extracts `ClientIdentity` (UID/PID) via `IServerConnection::GetClientIdentity()`; assigns a
  `session_id` per connection; drives cleanup-on-disconnect.
- `service_discovery_message_passing_client.{h,cpp}` — application-side client library; wraps
  `SendWaitReply`/subscription callbacks.
- `service_discovery_registry.{h,cpp}` — in-memory registry; creation lock
  (`creation_locks_: map<ServiceKey, session_id>`), usage lock (`UsageLockState` with exclusive
  owner + up to 32 shared owners).
- `service_discovery_protocol.{h,cpp}` — binary codec; fixed-size buffers
  (`kMaxRegistrationsPerService`≈32, `kMaxEndpointAddressSize`=256,
  `kMaxResponsePayloadSize`/`kMaxNotificationPayloadSize` computed from those); silent truncation
  via `PushRegistration()` early-exit when a response doesn't fit — evidence of the "fixed max size,
  no transport chunking" constraint in practice.
- `service_discovery_compat.{h,cpp}` — adapts the daemon/client to `mw/com`'s existing
  `impl::IServiceDiscovery` API (`OfferService`→`Register`, etc.). This is the LoLa integration
  glue whose right home is an open question (see `problem_statement.md`).
- `dependability/` — to-be-discarded TRLC/PlantUML; useful only as an index of concerns already
  thought about (see requirement/failure-mode names in the bootstrap subagent report), not as
  wording to reuse.

## Integrator: `score/mw/com/`
- `impl/i_service_discovery.h`, `impl/service_discovery.{h,cpp}` — binding-independent façade
  used by `SkeletonBase`/`ProxyBase`.
- `impl/i_service_discovery_client.h` — binding-specific client interface a rewritten SD must be
  adaptable to (via a compat layer, wherever it lives).
- `impl/bindings/lola/skeleton.h`/`.cpp`, `impl/bindings/lola/proxy.h` — **actual current**
  flock-based existence/usage marker file implementation (not in `service_discovery/`):
  `score::memory::shared::ExclusiveFlockMutex`/`SharedFlockMutex`/`LockFile`.
- `impl/bindings/lola/partial_restart_path_builder.cpp` — marker file path layout under
  `/tmp/mw_com_lola/partial_restart/...`.
- `impl/bindings/lola/service_discovery/client/service_discovery_client.h` — inotify-driven
  discovery watching those marker files; the mechanism SD's daemon-based notifications replace.
- `dependability/software_architectural_design/partial_restart/README.md` — design notes on the
  marker-file mechanism and why its directory layout matters for inotify performance.

## Dependency: `score/message_passing/`
- `service_protocol_config.h` — `max_send_size`/`max_reply_size`/`max_notify_size`, fixed per
  protocol kind at setup time.
- `client_connection.cpp` — `Send`/`SendWaitReply` enforce `max_send_size_`; `EMSGSIZE` on
  overflow; cannot call `SendWaitReply` from a callback thread (`EAGAIN`).
- `unix_domain/unix_domain_server.cpp`, `qnx_dispatch/qnx_dispatch_server.cpp` — reply/notify size
  checks, `EMSGSIZE` on overflow; QNX `Notify` can also return `ENOBUFS` if the notify pool is
  full.
- No chunking/fragmentation exists at the transport layer — any batching/splitting strategy is an
  application-protocol concern for SD to define itself.

## Process references
- `.github/skills/rules-score/SKILL.md` — lifecycle orchestrator this bootstrap follows.
- `.github/skills/score-requirements/SKILL.md`, `.github/skills/score-architecture/SKILL.md`,
  `.github/skills/score-safety-analysis/SKILL.md`, `.github/skills/score-testing/SKILL.md` — the
  four mechanical skills invoked by later lifecycle steps.
