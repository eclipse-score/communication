# Eclipse Score Communication - Message Passing Library Architecture

## Public APIs & Core Interfaces
Located in `/home/qxx6787/projects/xpad/eclipse-score-communication/score/message_passing/`

### Client-Side Interfaces
- **IClientConnection** [i_client_connection.h](i_client_connection.h)
  - `Send()` - fire-and-forget async
  - `SendWaitReply()` - blocking request-reply
  - `SendWithCallback()` - async request-reply with callback
  - States: Starting → Ready → Stopping → Stopped
  - StopReason enum for disconnection diagnosis
  
- **IClientFactory** [i_client_factory.h](i_client_factory.h)
  - `Create(ServiceProtocolConfig, ClientConfig)` → unique_ptr<IClientConnection>
  - ClientConfig: max_async_replies, max_queued_sends, fully_ordered, truly_async, sync_first_connect
  - Shared via std::shared_ptr between factory and connections

### Server-Side Interfaces
- **IServer** [i_server.h](i_server.h)
  - `StartListening(ConnectCallback, DisconnectCallback, MessageCallback, MessageCallback)`
  - `StopListening()`
  
- **IServerFactory** [i_server_factory.h](i_server_factory.h)
  - `Create(ServiceProtocolConfig, ServerConfig)` → unique_ptr<IServer>
  - ServerConfig: max_queued_sends, pre_alloc_connections, max_queued_notifies

- **IServerConnection** (referenced in server_types.h)
  - `Reply()`, `Notify()`, `RequestDisconnect()`
  - `GetClientIdentity()` → ClientIdentity{pid, uid, gid}
  - `GetUserData()` → UserData variant

### Connection Handlers & User Data
- **IConnectionHandler** [i_connection_handler.h](i_connection_handler.h)
  - Per-connection virtual methods alternative to server-wide callbacks
  - `OnMessageSent()`, `OnMessageSentWithReply()`, `OnDisconnect()`
  
- **server_types.h**: UserData = variant<void*, uintptr_t, unique_ptr<IConnectionHandler>>

### Service Configuration
- **ServiceProtocolConfig** [service_protocol_config.h](service_protocol_config.h)
  - `identifier` (string_view) - service name in namespace
  - `max_send_size`, `max_reply_size`, `max_notify_size` - message size limits

## Transport Abstraction (Linux vs QNX)

### Linux: Unix Domain Sockets
Location: `score/message_passing/unix_domain/`

- **UnixDomainEngine** [unix_domain_engine.h](unix_domain_engine.h#L37)
  - Shared resources: Signal, Socket, SysPoll, Unistd (OS wrappers)
  - Background thread with poll() loop
  - Implements ISharedResourceEngine
  
- **UnixDomainClientFactory** [unix_domain_client_factory.h](unix_domain_client_factory.h)
  - Wraps UnixDomainEngine
  - `GetEngine()` → shared_ptr<UnixDomainEngine>
  
- **UnixDomainServerFactory** [unix_domain_server_factory.h](unix_domain_server_factory.h)
  - Wraps UnixDomainEngine
  - `GetEngine()` → shared_ptr<UnixDomainEngine>
  
- **UnixDomainSocketAddress** [unix_domain_socket_address.h](unix_domain_socket_address.h#L23)
  - SOCK_SEQPACKET/SOCK_STREAM with custom packetizer
  - Supports abstract socket paths: `UnixDomainSocketAddress(identifier, isAbstract=true)`

### QNX: Native Dispatch Messaging
Location: `score/message_passing/qnx_dispatch/`

- **QnxDispatchEngine** [qnx_dispatch_engine.h](qnx_dispatch_engine.h#L42)
  - OS Resources: Channel, Dispatch, Fcntl, IoFunc, Signal, Timer, SysUio, Unistd
  - Background dispatch thread with pulse handlers
  - Implements ISharedResourceEngine
  - ResourceManagerServer: base class for QNX resource manager pattern
  
- **QnxDispatchClientFactory** [qnx_dispatch_client_factory.h](qnx_dispatch_client_factory.h)
  - `GetEngine()` → shared_ptr<QnxDispatchEngine>
  
- **QnxDispatchServerFactory** [qnx_dispatch_server_factory.h](qnx_dispatch_server_factory.h)
  - `GetEngine()` → shared_ptr<QnxDispatchEngine>
  
- **QnxResourcePath** [qnx_resource_path.h](qnx_resource_path.h#L21)
  - Static prefix: `/mw_com/message_passing/`
  - Max identifier length: 256 chars
  - Buffer: prefix + identifier + null terminator

### Platform Selection
[client_factory.h](client_factory.h), [server_factory.h](server_factory.h)
```cpp
#ifdef __QNX__
  using Engine = QnxDispatchEngine;
  using ClientFactory = QnxDispatchClientFactory;
  using ServerFactory = QnxDispatchServerFactory;
#else
  using Engine = UnixDomainEngine;
  using ClientFactory = UnixDomainClientFactory;
  using ServerFactory = UnixDomainServerFactory;
#endif
```

## Shared Resource Engine Abstraction
[i_shared_resource_engine.h](i_shared_resource_engine.h#L30)

Extension points for custom implementations:
- `GetMemoryResource()` - PMR support
- `GetLogger()` - logging callback
- `TryOpenClientConnection(identifier)` - service lookup (returns fd)
- `SendProtocolMessage(fd, code, message)` - transport send
- `ReceiveProtocolMessage(fd, code)` - transport receive
- `RegisterPosixEndpoint()`, `UnregisterPosixEndpoint()` - poll/dispatch integration
- `EnqueueCommand()` - timer queue for timeouts
- `CleanUpOwner()` - resource cleanup

## Bazel Targets & Dependencies
[BUILD](BUILD)

### Main targets (platform-selected)
- `:message_passing` - public API (selects qnx_dispatch or unix_domain)
- `:common_headers` - shared interface definitions
- `:message_passing_common` - client connection implementation
- `:message_passing_unix_domain` - Linux implementation
- `:message_passing_qnx_dispatch` - QNX implementation

### Test targets
- `unit_tests` (Linux): client_connection_test, unix_domain_test, qnx_resource_path_test
- `unit_tests_qnx` - QNX-specific tests
- Integration: `test/integration_test:test_stress` (stress_test.py)

### Unit design (dependability)
- `:client_connection` unit with implementation → :message_passing_common
- `:unix_domain` unit → :message_passing_unix_domain
- `:qnx_dispatch` unit (no runtime implementation in core)

## Test Patterns & Examples

### Example Usage
[test/server.cpp](test/server.cpp), [test/client.cpp](test/client.cpp)

Server:
1. Create ServerFactory
2. Create IServer via factory
3. Register connect/message callbacks
4. `StartListening()` → background dispatch
5. Callbacks run on library thread

Client:
1. Create ClientFactory
2. Create IClientConnection via factory
3. `Start()` → async connection attempts
4. Check state/stop_reason via polling
5. `SendWaitReply()` or `Send()` when Ready
6. `Stop()` to clean up

### Integration Tests
- [test/integration_test/stress_test.py](test/integration_test/stress_test.py)
  - Multi-thread stress with binary messages
  - Uses pkg_filegroup → filesystem integration

### Unit Tests
- [unix_domain_server_test.cpp](unix_domain_server_test.cpp) - server lifecycle, port conflicts
- [unix_domain_server_to_client_test.cpp](unix_domain_server_to_client_test.cpp) - bidirectional communication
- [qnx_dispatch_*_test.cpp](qnx_dispatch_engine_test.cpp) - QNX dispatch specifics

## Layering & Extension Points for Service Discovery Daemon

### Current architecture
1. Service identifier is string passed to factory
2. No explicit discovery - direct by-name connection attempt
3. Transport-specific namespace (abstract socket on Linux, resource path on QNX)

### Extensibility points for service discovery:

1. **ISharedResourceEngine abstraction**
   - Implement custom engine subclass
   - Override `TryOpenClientConnection(identifier)` for service registry lookup
   - Return fd from custom lookup mechanism

2. **Factory pattern**
   - Create custom ClientFactory/ServerFactory implementations
   - Could wrap standard factories + inject service discovery layer
   - Share custom Engine instance

3. **Registration layer**
   - Add `RegisterPosixEndpoint()` calls in ServerFactory.Create()
   - Endpoint callbacks can trigger service registry updates
   - Use `CleanUpOwner()` for cleanup on server destruction

4. **Protocol negotiation**
   - ServiceProtocolConfig could be extended with discovery metadata
   - Add optional discovery_uri field pointing to service registry
   - Connect to registry first, then to resolved service endpoint

5. **QNX resource manager pattern**
   - Extend QnxDispatchEngine::ResourceManagerServer
   - Implement custom _io_open() handler for service lookup
   - Use NOREPLY to postpone connection until registry resolved

## Memory Management
- PMR-aware (polmorphic memory resource)
- Factory owns shared resources (Engine)
- Connections hold shared_ptr<Engine> - lifetime extends until last connection destroyed
- Pre-allocation support in ServerConfig.pre_alloc_connections
- Monotonic buffer resource support for safe applications

- C++ standard in this workspace is C++17 for these targets; avoid C++20-only syntax like defaulted comparison operators in new module code.

- Integration tests that need multiple apps in one rootfs should define an integration_test-local pkg_filegroup named filesystem combining several *-pkg targets (example pattern now used in score/mw/com/test/bigdata/integration_test/BUILD for bigdata + service discovery daemon app).
- For daemon-backed scenarios, start the daemon process first in the Python integration test context manager so provider/consumer binaries connect to an already listening service endpoint.

- QNX async Rust stream integration (`com_api_async_api_test.py`) can fail during daemon WrappedProcess teardown with SSH banner errors; launching service discovery daemon via `target.execute_async(...)` and letting QEMU fixture teardown handle process cleanup avoided this flake in repeated uncached runs.
- QNX `data_slots_read_only` integration flake was also caused by daemon WrappedProcess `stop()` teardown over SSH. Starting service discovery daemon once via `target.execute_async(...)` in `test_data_slots_read_only.py` and avoiding per-phase daemon context teardown stabilized repeated uncached runs.
- 2026-09-17: `message_passing` actualize cycle `changes/2026-09-01-client-identity-and-userdata-docs/` is closed (Step 7). Logged the 2026-09-16 `memory_and_size_limits_findings.md` doc into `research/backlog.md` as deferred technical debt (not actioned).
- 2026-09-17: Opened new cycle `changes/2026-09-17-architecture-completeness-for-fta-redo/` (Step 0/1 drafted, checkpoint pending human answer). Trigger: human says current FTA (`dependability/safety_analysis/fta_*.puml`) isn't meaningful and linking it to requirements is pointless; wants FTA redone but only after architecture (esp. sequence diagrams) is complete. Impact analysis found root cause is ONE layer up from FTA: only 1 of 8 FTAs (`fta_message_not_delivered_correctly`) has all `$BasicEvent` aliases backed by a real `ControlMeasure` in `control_measures.trlc`; the other 7 (~20 basic events) reference nonexistent control measures AND aren't grounded in any architecture artifact — `server_client_sequence.puml`/`client_connection_activity_diagram.puml` only show the happy path (no connection-refused/EAGAIN, no message-too-big, no send/notify-queue-exhaustion, no mid-request disconnect, no RequestDisconnect misuse, no timing/watchdog). Plan: this cycle = extend sequence/activity diagrams with real failure paths grounded in headers (`i_client_connection.h` etc.)/`client-server.md`; FTA/control-measures redo explicitly DEFERRED to a follow-on cycle (don't fix FTA before architecture backs it, per rules-score-actualize Core Principle 3 — upward trace before patching symptom). Waiting on human answers to 3 open questions in `change_request.md` (extend-in-place vs new diagram file; candidate 9-scenario list complete?; auto-continue to FTA redo or wait for new cycle) before touching any `.puml`.
- 2026-09-17 (wrap-up follow-up): safety_concept_notes.md refined per human answers: (a) fail-stop scope is NOT uniform — library preallocates memory as early as possible, is more willing to terminate at startup if preallocation fails, tries to fail only the affected activity at runtime (e.g. FD exhaustion for a NEW connection shouldn't break EXISTING ones), but can't fully rule out later termination (e.g. own memory_resource exhausted), and explicitly does NOT require the integrator to terminate on its behalf (no such AoU); exact per-failure-point boundary is unresolved. (b) peer-misbehavior containment is explicitly INCONSISTENT today across the codebase: protocol errors → drop connection; unhandled Notify() → ignore single message, channel stays alive; almost everywhere else → implicitly assumes required callback is already registered (AoU-style, not a runtime mechanism). This inconsistency itself is an open future decision point, not to be papered over with one invented uniform rule.
- 2026-09-19: Cycle `changes/2026-09-19-assumed-system-requirements-rewrite/` CLOSED (Step 7, evidence_bundle.md written). Went through 6 rounds of human feedback before transcription: round 1 draft (failure-handling principles) was rejected as wrong altitude for AssumedSystemReq (belongs at FeatReq/CompReq); round 2 reframed around capability-sufficiency (happy flow / production error detection / integration-time misconfig detection) per human's explicit framing ("would an integrator comparing IPC libraries conclude this one is capable of X"); round 3 merged one-way+notify capabilities, moved certified-transport/bounded-memory/singleton-free/mockability under one new `QnxAsilBQualifiedImplementation` ASIL-B meta-requirement instead of separate promotions, split multi-platform support into `CrossPlatformAbstraction`(QM)/`QnxAsilBQualifiedImplementation`(B) mirroring the existing SafetyCertifiedTransportMechanismUnderQNX(B)/TransportMechanismOnLinux(QM) precedent, added `PeerIdentityInformationForAccessControl`; round 4 clarified `OSIndependentAPI`(B, API-contract-is-OS-independent) is NOT the same thing as "QM implementations allowed on non-QNX OSes" (new, deferred) — no split needed; round 5 killed the word "fire-and-forget" (misleading — send queues are explicitly bounded/configurable, not unbounded) and simplified wording. Final 9 AssumedSystemReq records: ClientServerCommunicationModel, PointToPointConnectionTopology, RequestReplyInteractionCapability, OneWayMessageDeliveryCapability, RuntimeFailureDetectability, IntegrationMisconfigurationDetectability, CrossPlatformAbstraction(QM), QnxAsilBQualifiedImplementation, PeerIdentityInformationForAccessControl. Retired SystemMessagingProtocol and the unverified SafeState/"safe-silent" Mitigation with NO replacement Mitigation record (per human: Mitigation/ControlMeasure content belongs in a future control_measures.trlc under the updated, not-yet-rebased @score_tooling schema, not in assumed_system_requirements.trlc). KEY PRECEDENT CONFIRMED (from 2026-09-01 cycle, reused here): a re-pin (updating a derived_from version pointer to a newer, same-identity parent) does NOT require bumping the referencing record's own version — only records whose OWN content (description/safety/derived_from target identity) actually changes get a version bump. Applied here: all 12 FeatReqs bumped 1→2 (their derived_from target genuinely changed to a new AssumedSystemReq), but all 31+4 CompReqs referencing them stayed at version=1 (pure @1→@2 pin update). `bazel test //score/message_passing/dependability/assumed_system/... //score/message_passing/dependability/requirements/...` — 4/4 PASSED. Deferred: Notify-specific FeatReq split (currently folded into OneWayMessageDeliveryCapability), new FeatReq for "QM implementations on non-QNX OSes" under CrossPlatformAbstraction, and the whole failure_modes/control_measures/fta_*.puml reconciliation (still blocked on the paused 2026-09-17-fta-redo-grounded-in-architecture cycle).
- 2026-09-17 (later same day): Cycle `changes/2026-09-17-fta-redo-grounded-in-architecture/` PAUSED (not closed). Human demonstrated by reading `ClientConnection::TryConnect()` in `client_connection.cpp` directly that `ServerHealthCheck`/`ClientRetryPolicy` (fta_ipc_channel_unavailable.puml) is a placeholder mismatched with reality: real mechanism is a capped-backoff retry loop (`connect_retry_ms_` grows up to `kConnectRetryMsMax`) that keeps retrying on EAGAIN/ECONNREFUSED/ENOENT, and stops immediately with a specific StopReason (kPermission for EACCES, kIoError otherwise) for anything else — no "health check" of any kind exists. This proved header/prose-grounded review (what the architecture-completeness cycle did) is NOT sufficient — FailureMode basic events must be re-derived by reading the actual .cpp code path, which is much more work and per human may need the original (unknown) author's input for intent not recoverable from code. Human also called out `SafeState="safe-silent"` in assumed_system_requirements.trlc as unverified/likely-invented ISO 26262 terminology, and dictated 8 numbered plain-language safety principles for message_passing's real intended behavior (report-error-when-API-allows / silent-when-not / preserve-ordering-unless-configured / send-failure-either-reported-or-channel-goes-silent-until-reconnect / per-channel-isolation / fail-fast-terminate-when-isolation-cant-be-guaranteed-e.g.-ENOMEM / proportionate-AoUs-for-same-process-misuse-dont-inflate-them / peer-misbehavior-must-not-crash-process-and-containment-choice-e.g.-ignore-one-spurious-Notify()-vs-mute-whole-channel-must-be-documented). Captured verbatim as authoritative ground truth in NEW file `score/message_passing/research/safety_concept_notes.md` (supersedes existing placeholder TRLC wording until reconciled — read this file before touching any FailureMode/ControlMeasure/AoU/fta_*.puml content in this component again). No .trlc/.puml edited as a result. Reconciling all 8 FailureModes this way is future work, possibly not even next session.
- 2026-09-24: Resumed paused cycle `changes/2026-09-17-fta-redo-grounded-in-architecture/` — did
  the code-level re-derivation `next_steps.md` called for (read `client_connection.cpp`/`.h` and
  `unix_domain/unix_domain_server.cpp` in full directly, subagent pass over QNX dispatch backend +
  remaining headers, spot-checked). Confirmed original Open Questions 1/2/3/6/8/9 exactly; REVISED
  4/5 (ServerHealthCheck+ClientRetryPolicy are ONE mechanism — TryConnect()'s capped-backoff retry:
  kConnectRetryMsStart=50, grows by *(1+1/3), capped kConnectRetryMsMax=5000, for EAGAIN/
  ECONNREFUSED/ENOENT; EACCES->kPermission, else->kIoError, both terminal no-retry — propose ONE
  consolidated ControlMeasure, not two). Surfaced 3 NEW judgement calls (added as Open Questions
  11-13 in change_request.md): (a) LifecycleOrderEnforcement may be partly a ControlMeasure not
  pure AoU — Send/SendWaitReply/SendWithCallback/Restart all explicitly check state and return
  EINVAL rather than misbehaving (detected+reported precondition); (b) BE_HandlerNotRegistered's
  exact behavior on an unset/empty score::cpp::callback invocation is unverified (external dep, not
  read this session) — do not assume graceful no-op; (c) BE_NotifyQueueExhausted has a real
  platform asymmetry: UnixDomainServer::ServerConnection::Notify() (confirmed by direct read) has
  NO queue at all (synchronous per-call send, size-checked only via max_notify_size_), while QNX
  dispatch has a genuine preallocated notify_pool_/ENOBUFS — propose QNX-scoped ControlMeasure only.
  Also confirmed: UnixDomainServer::ProcessConnect() has `#ifdef __QNX__` branch hardcoding
  ClientIdentity{0,0,0} — meaning Unix Domain Sockets CAN be compiled/used on QNX directly (bypassing
  the "official" server_factory.h QnxDispatchServerFactory alias), so the identity gap is reachable,
  not dead code. ConnectCallback's UserData has zero validation (confirmed, supports consolidating
  the two ConnectCallback* basic events). Updated impact_analysis.md/change_request.md/work_log.md/
  next_steps.md in the cycle dir; did NOT write any .trlc/.puml — still waiting on human to confirm
  13 total Open Questions before Step 2. Sequence-diagram/static_design.puml/private_api.puml
  reconciliation (the 54 findings from the 2026-09-23 sibling cycle) deliberately deferred until
  AFTER FTA content is confirmed (diagram section names referenced by basic-event descriptions need
  to be final first).
- 2026-09-24 (correction, same day): human caught that the above re-derivation made a methodological
  error — `UnixDomainServer`/`UnixDomainEngine` are QM-only (`TransportMechanismOnLinux`); the FTA's
  ASIL-B scope (`integrity_level = "B"`) is carried only by the QNX dispatch backend
  (`SafetyCertifiedTransportMechanismUnderQNX`). Grounding ASIL-B `ControlMeasure` wording in
  `unix_domain_server.cpp` reads (as done above) instead of `qnx_dispatch/*.cpp` directly is wrong;
  redo against QNX dispatch source before finalizing. The `BE_NotifyQueueExhausted` "platform
  asymmetry"/QNX-scoping proposal is likely moot once Unix Domain is correctly recognized as
  out-of-FTA-scope; also "no queue at all" for Unix Domain `Notify()` overstated it — the OS Unix
  domain socket itself has an implicit kernel send buffer, just no *library-managed* queue.
  Durable methodology point added to `safety_concept_notes.md` principle 9: an `AoU` (uncontrolled
  root cause) and a parallel, partial mitigation are NOT mutually exclusive — e.g. `Send()`'s
  `TimingSupervision` AoU (no timing guarantee) can coexist with noting its internal queue +
  background-thread dispatch as a real partial mitigation against the `DelayedFunction` HAZOP
  guideword (confirmed present in the real `@score_tooling` `score_requirements_model.rsl` at
  `bazel/rules/rules_score/trlc/config/score_requirements_model.rsl` — distinct from `TooLate`, not
  yet used in `failure_modes.trlc`; the vendored `third_party/score_requirement_model/` copy in
  this repo is stale/incomplete and should not be treated as the schema source of truth — resolve
  via the `@score_tooling` external Bazel repo instead). Most importantly: **human explicitly
  deprioritized the whole `2026-09-17-fta-redo-grounded-in-architecture` cycle** behind finishing
  requirements/API-surface work — do not resume it just because `research/backlog.md` references
  it; check current priority first. `next_steps.md`/`work_log.md` in that cycle dir rewritten
  accordingly; `research/backlog.md`'s 2026-09-23 entry annotated to no longer read as a priority
  signal.
- 2026-09-23: Reviewed rebased `public_api.puml` (commits `304ac462`→`ba1e1d96`→`d3b34c89`, all same-day) against real headers + requirements + the mechanical `component_public_api` validator (@score_tooling). Findings: (1) Method-level fidelity is now GOOD — every method shown for `IClientConnection`/`IServer`/`IServerConnection`/`IServerFactory`/`IClientFactory`/`IConnectionHandler`/concrete `ClientFactory`/`ServerFactory`/`Engine` (ctors, `GetEngine()`, `GetDefaultOsResources()`) matches real code (`i_client_connection.h`, `i_server.h`, `i_server_connection.h`, `i_client_factory.h`, `i_server_factory.h`, `qnx_dispatch_client_factory.h`/`unix_domain_client_factory.h` (+server), `qnx_dispatch_engine.h`/`unix_domain_engine.h`) — old version (pre `ba1e1d96`) had invented methods (`CreateClientConnection`, `SendWithReply`, `Set*Callback`) and was missing `IServer`/`IClientFactory`/`IServerFactory`/`IServerConnection`/`IConnectionHandler` entirely. (2) TOOL-VERIFIED BUG (still present, pre-existing, not fixed by this rebase): `bazel build //score/message_passing/dependability/software_architectural_design:message_passing_architectural_design` succeeds but its "Running architectural-design validation" step reports 2 FAILED errors (visible in the build log, non-blocking today but real per `component_public_api.md` validator spec at `external/score_tooling+/validation/core/docs/specifications/component_public_api.md`): `static_design.puml` declares two top-level interfaces bound to the `<<SEooC>>` boundary — `interface "score::message_passing" as message_passing_public_api` (provided, via portout) and `interface "OS" as os` (required, via portin) — but `public_api.puml` has never declared matching TOP-LEVEL interface entities with those exact aliases (everything in `public_api.puml` lives inside `namespace score::message_passing { ... }`, one level too deep — validator only reads top-level interfaces; matching is by alias/name, case-sensitive, per the spec's worked example). Needs a top-level `message_passing_public_api` interface AND a top-level `os` interface added to `public_api.puml`, or reconsider whether `os` should be modeled as a required top-level SEooC interface at all in `static_design.puml` (semantically odd to force OS-consumption into the "public API" diagram, but the validator treats required and provided top-level SEooC-bound interfaces identically — no special-casing). (3) Requirement-coverage gaps found by cross-referencing `component_requirements.trlc` (254 lines, read in full) against the diagram: (a) ZERO `CompReq` exists anywhere for `Engine`/`ClientFactory`/`ServerFactory` as concrete classes — only the interface-level `Create()` method is covered (`ClientFactoryCreateAPI`/`ServerFactoryCreateAPI`); the newly-diagrammed constructors (`memory_resource`-only, `shared_ptr<Engine>`-sharing) and `GetEngine()` accessor have no CompReq at all. The engine-sharing capability (`shared_ptr<Engine>` ctor + `GetEngine()`, lets one process share a single background-thread/engine between a `ClientFactory` and a `ServerFactory` instead of each owning its own) plausibly traces to `SingletonFreeImplementation`/`AllowsBoundedMonotonicMemoryAllocation` but nothing currently says so. (b) `GetDefaultOsResources()`: recommend NO dedicated CompReq — it's a pure implementation helper with no independent behavioral contract, existing only to supply the default argument of `QnxDispatchEngine`'s convenience ctor (verified: `QnxDispatchEngine(memory_resource, logger) : QnxDispatchEngine(memory_resource, GetDefaultOsResources(memory_resource), logger) {}`). BUT flagged as an internal-consistency smell in the diagram: `public_api.puml`'s `Engine` box shows `GetDefaultOsResources()` while deliberately NOT showing the `OsResources`-parameterized constructor it exists to feed (that ctor is QNX-only/test-only — confirmed via `qnx_dispatch_engine_test.cpp` using `MoveMockOsResources()`; `UnixDomainEngine` has ONLY the single `(memory_resource, logger)` ctor, no `OsResources`-accepting overload at all, confirmed by reading the full header — asymmetric platform capability, not a bug, just undocumented). Since `public_api.puml` appears to intentionally document the OS-independent/alias-level common contract (matches `OSIndependentAPI` FeatReq), showing a static method whose only caller-visible purpose (partial OS-resource override/mock injection) is invisible in this same diagram is inconsistent — recommend either dropping `GetDefaultOsResources()` from `public_api.puml` or accepting that Engine-level (not just `ISharedResourceEngine`-level) mock injection is QNX-specific and belongs in a QNX-scoped note, paralleling the existing `SafetyCertifiedTransportMechanismUnderQNX`-style platform-scoped CompReq naming. Also: the real Engine-level OS-resource mock-injection capability (as opposed to `ClientConnectionMockInjectionForTesting`, which is about `ClientConnection` accepting `ISharedResourceEngine*`, a different layer) has NO CompReq tracing to `AllowsResourceMockInjectionForTesting` at all — gap. (c) Asymmetric "API Requirements" coverage on `IClientConnection`: `Send`/`SendWaitReply`/`SendWithCallback`/`GetState` each have a dedicated API CompReq, but `Start()`/`Restart()`/`Stop()`/`GetStopReason()` — equally public methods, equally shown in the diagram — have NONE (only indirectly implied by the behavior-level `ClientConnectionMaintainsStateMachine`/`ClientConnectionStateCallbackInvocation`). `IServer` by contrast got explicit `IServerStartListeningAPI`/`IServerStopListeningAPI` for its equivalent lifecycle methods — worth the same treatment for `IClientConnection` for symmetry. (4) Side note, out of scope of this rebase but noticed: `private_api.puml` is stale — still uses placeholder `Dispatch::QNX::Client/Server` interfaces with invented method names (`Connect()`, `SendMessage()`, `Dispatch()`, `Accept()`, `Listen()`) that don't match any real class (`QnxDispatchEngine`, `UnixDomainEngine`) — diverges further from reality now that `public_api.puml` uses accurate real names.
- 2026-09-24: Cycle `changes/2026-09-24-client-interface-feature-requirement/` OPENED AND CLOSED
  same session (Step 0→7, evidence_bundle.md written). Fixed the `ServerInterface`-exists/
  `ClientInterface`-doesn't asymmetry flagged in backlog.md (2026-09-23 cycle): added `FeatReq
  ClientInterface` (version=1, derived_from=[ClientServerCommunicationModel@1], safety=Asil.B) to
  feature_requirements.trlc mirroring ServerInterface exactly; re-pinned 7 CompReqs from
  OSIndependentAPI@2 to ClientInterface@1, each version bumped 1→2 (identity change, not a
  same-identity pin, so this needed a version bump per established precedent):
  ClientConnectionMaintainsStateMachine, ClientConnectionStateCallbackInvocation,
  IClientConnectionGetStateAPI, IClientConnectionGetStopReasonAPI, IClientConnectionStartAPI,
  IClientConnectionStopAPI, IClientConnectionRestartAPI. Left ClientFactoryCreateAPI/
  ServerFactoryCreateAPI pinned to OSIndependentAPI@2 deliberately (about Create()'s OS-independence,
  not "there is a client/server interface" — symmetric, not a gap). Validated:
  `bazel test //score/message_passing/dependability/requirements/...` 2/2 PASSED; also built
  `//score/message_passing/dependability:dependable_element_message_passing` end-to-end
  successfully (lobster-trlc: 15 feature_requirements + 37 component_requirements items, no
  dangling refs). backlog.md's originating entry marked resolved. Note: this was done as a
  reasonably fast, low-ambiguity mechanical fix (mirrors an already-reviewed pattern 1:1) without a
  separate human checkpoint before editing — appropriate for well-understood, low-judgment
  requirements work, unlike the paused FTA cycle's ASIL-B safety-judgment calls which do need
  upfront confirmation.
- 2026-09-24: Cycle `changes/2026-09-24-diagram-reconciliation-54-findings/` opened and driven to a
  clean (0-finding) `bazel build` of `message_passing_architectural_design` — see
  `/memories/repo/score-documentation-conventions.md` for the full durable technical writeup (real
  architecture corrections: `ISharedResourceEngine` is `ClientConnection`-only, not shared with
  server-side units; `os` interface grounded in real `score::os::*` classes; a real `puml_cli`
  resolver bug found and worked around). Drive-by: removed genuinely-unused
  `listener_command_`/`listener_endpoint_` members from `QnxDispatchServer` (header+cpp, both
  `eclipse-score-communication` and `copybara-export` copies; left the legacy `bmw`/`amp`
  `safe-posix-platform` fork untouched) per human's direct code-reading catch. Cycle CLOSED
  2026-09-24 (human accepted evidence_bundle.md; 0 findings confirmed on re-check).


