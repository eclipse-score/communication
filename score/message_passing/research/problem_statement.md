# Message Passing — Baseline Snapshot (orientation, not source of truth)

> **This file is reverse-documented orientation material**, written per the `rules-score-actualize`
> skill's "if `research/` does not exist yet" step. It is a read-only summary of an already-existing,
> trusted `dependable_element` (`score/message_passing/dependability/`), not a fresh Step-0-style
> derivation against a discardable PoC. The frozen TRLC records and PlantUML diagrams under
> `dependability/` remain the authoritative source of truth; this file exists only so a future
> actualization cycle can orient itself quickly without re-reading the whole tree. If this file and
> the frozen artifacts ever disagree, the frozen artifacts win — treat the disagreement as a finding
> for `backlog.md`, not as license to trust this file instead.

## Current maturity / integrity level

From `dependability/BUILD`, `dependable_element_message_passing`:
- `integrity_level = "B"` (ASIL B)
- `maturity = "development"` (drift/coverage/GWT violations are warnings, not hard build failures,
  today)

## Terminology already in use

- **Service Identifier** — the name a `Server` is addressable by in the service-address namespace
  (`ServiceProtocolConfig::identifier`).
- **Service Protocol** — the application-level contract (max message/reply/notify sizes) that both
  a `Server` and the `ClientConnection`s that talk to it must agree on; carried by
  `ServiceProtocolConfig`.
- **Server** (`IServer`) — a named entity that accepts inter-process traffic addressed to one
  Service Identifier; produced by `IServerFactory::Create`.
- **Server Connection** (`IServerConnection`) — the server-side endpoint object for one
  successfully-established client connection; not created directly by the user, lifetime managed
  by the library. Provides `GetClientIdentity()`, `GetUserData()`, `Reply()`, `Notify()`,
  `RequestDisconnect()`.
- **Client Connection** (`IClientConnection`) — the client-side endpoint object toward one Server;
  produced by `IClientFactory::Create`. Owns a state machine (`Starting`, `Ready`, `Stopping`,
  `Stopped`) with an associated `StopReason` (`kNone`, `kInit`, `kUserRequested`, `kPermission`,
  `kClosedByPeer`, `kIoError`, `kShutdown`).
- **Client Factory** / **Server Factory** (`IClientFactory` / `IServerFactory`) — encapsulate the
  OS-dependent transport implementation, configuration, and shared resources (background thread,
  command queue, memory resource); outlive every connection/server object they produce.
- **Connection Handler** (`IConnectionHandler`) — optional per-connection user object
  (`OnMessageSent`, `OnMessageSentWithReply`, `OnDisconnect`) that, when present as `UserData`,
  replaces the server-wide callbacks for that one connection.
- Packet types in the abstracted wire protocol: `SEND` (fire-and-forget), `REQUEST`
  (send-and-wait/send-with-callback), `REPLY`, `NOTIFY`.
- "Point-to-point" — the design explicitly limits connections to 1:1 (client connection ↔ server
  connection); N:M is out of scope.

## System slice

A same-host, OS-independent, point-to-point client-server IPC abstraction over two backends behind
one API:
- **Linux**: Unix Domain Sockets (`unix_domain/`), used for host testing and as the Linux transport.
- **QNX**: native QNX message passing / `dispatch` with `pulse_attach()` (`qnx_dispatch/`), the
  ASIL-B-safety-certified transport on target.

Both backends implement the same `IClientConnection` / `IServer` / `IServerConnection` /
`IClientFactory` / `IServerFactory` interfaces so callers are transport-agnostic. The design is
singleton-free, supports bounded monotonic memory allocation (pre-allocated connection objects,
ring-buffer send queues sized at construction time), and allows resource-mock injection via
`ISharedResourceEngine` for unit testing.

## Existing requirement index (name + one-line gist)

### Assumed System Requirements (`assumed_system/assumed_system_requirements.trlc`)
- `SystemMessagingProtocol` (AssumedSystemReq, ASIL B) — system needs a client-server IPC
  messaging mechanism across process boundaries respecting ISO 26262 failure modes.
- `SafeState` (Mitigation, ASIL B) — the safe state of the system is *safe-silent*.

### Assumptions of Use (`assumed_system/aous.trlc`)
- **Placeholder only** — contains a single `TODO` comment and one literal `ExampleAoU` record with
  placeholder description/note text (`mitigates = "FailureModeName"`, which is not a real failure
  mode name in this component). No real AoUs have been authored yet. Flagged in `backlog.md`.

### Feature Requirements (`requirements/feature_requirements.trlc`), all `derived_from
SystemMessagingProtocol@1`
- `ServerInterface` (B) — server registers connection handlers and processes incoming requests.
- `OSIndependentAPI` (B) — OS-independent API over OS-native IPC mechanisms.
- `SafetyCertifiedTransportMechanism` (B) — QNX implementation uses a safety-certified transport.
- `PointToPointConnections` (B) — only 1:1 connections; N:M explicitly excluded.
- `SmallDataLowLatencyCommunication` (QM) — low-latency small-data communication.
- `SynchronousUnidirectionalCommunication` (B) — blocking fire-and-forget send.
- `SynchronousBidirectionalCommunication` (B) — blocking send-and-wait-for-reply (`SendWaitReply`).
- `AsynchronousUnidirectionalCommunication` (B) — non-blocking send, no delivery guarantee.
- `SingletonFreeImplementation` (B) — no singletons in the design.
- `AllowsBoundedMonotonicMemoryAllocation` (B) — bounded monotonic allocation.
- `AllowsResourceMockInjectionForTesting` (B) — resource mock injection for tests.

### Component Requirements (`requirements/component_requirements.trlc`), grouped by section
- *Behaviour Requirements*: `ServerCallbacksAreSequential`, `ServerProcessesSinglePendingRequest`,
  `ClientConnectionMaintainsStateMachine`, `SynchronousSendBlocksUntilServerReceives`,
  `AsynchronousSendReturnsAfterLocalAcceptance`, `SendWaitReplyBlocksUntilServerReply`,
  `MessageOrderPreservationPerConnection`, `SingleServerInstancePerServiceIdentifier`.
- *API Requirements*: `IServerStartListeningAPI`, `IServerStopListeningAPI`,
  `IClientConnectionSendAPI`, `IClientConnectionSendWaitReplyAPI`,
  `IClientConnectionSendWithCallbackAPI`, `IServerConnectionReplyAPI`,
  `IServerConnectionNotifyAPI`, `ClientFactoryCreateAPI`, `ServerFactoryCreateAPI`,
  `IClientConnectionGetStateAPI`.
- *Server Unit Requirements*: `ServerPreallocatesConnectionObjects`,
  `ServerRingBufferQueueSizeConfigurable`, `ServerConnectionRefusal`,
  `ServerIConnectionHandlerDispatch`.
- *Client Unit Requirements (client_connection)*: `ClientConnectionSendQueuePreallocation`,
  `ClientConnectionSharedResourceEngineInjection`, `ClientConnectionMockInjectionForTesting`,
  `ClientConnectionSendFailsWhenStopped`, `ClientConnectionSendWaitReplyFailsWhenStopped`,
  `ClientConnectionSendWithCallbackFailsWhenStopped`, `ClientConnectionStateCallbackInvocation`.
- All entries above are ASIL B, `version = 1`.

### External Component Requirements (`requirements/external_component_requirements.trlc`)
(requirements towards the system / environment)
- `SafetyCertifiedTransportMechanismUnderQNX` (B) — QNX uses QNX-message-passing.
- `TransportMechanismOnLinux` (B) — Linux uses Unix Domain Sockets.
- `OSProvidedSenderIdentity` (B) — server identifies sender by OS-provided UID.
- `UnforgableSenderIdentity` (B) — UID used for identification cannot be forged by the client.

### Failure Modes (`safety_analysis/failure_modes.trlc`), all ASIL B, `version = 1`
- `IpcChannelUnavailable` — channel cannot be established/maintained.
- `MessageNotDeliveredCorrectly` — message lost/partial/corrupted, or routed to wrong instance.
- `NotificationNotDelivered` — server→client notification lost/partial/late/corrupted/spurious.
- `ServerMessageHandlingFailure` — server handler fails to process/reply.
- `MessageTimingViolated` — message/reply delivered outside timing bounds (FTTI risk).
- `IpcApiMisuseOrLifecycleViolation` — API called at wrong lifecycle stage/context.
- `ConnectionContextDataWrong` — identity/context data for a connection absent/incorrect.
- `StateMachineError` — `GetState`/`GetStopReason` returns a state inconsistent with reality.

Each failure mode has a matching `fta_*.puml` diagram (`fta_ipc_channel_unavailable.puml`,
`fta_message_not_delivered_correctly.puml`, `fta_notification_not_delivered.puml`,
`fta_server_message_handling_failure.puml`, `fta_message_timing_violated.puml`,
`fta_ipc_api_misuse_or_lifecycle_violation.puml`, `fta_connection_context_data_wrong.puml`,
`fta_state_machine_error.puml`).

### Control Measures (`safety_analysis/control_measures.trlc`)
Only **one** FTA's control measures are authored so far — all three mitigate
`MessagePassing.MessageNotDeliveredCorrectly`:
- `OsIpcFaultHandling` (B) — check the return value of every OS call.
- `SendBufferArgumentValidation` (B) — validate Send/SendWaitReply/SendWithCallback arguments
  before any IPC operation.
- `BE_MessageTooBig` (B) — enforce `max_send_size`/`max_reply_size`/`max_notify_size` limits.
- `BE_SendQueueExhausted` (B) — bounded pre-allocated send-queue pool, error (not block/drop) when
  exhausted.

The other seven failure modes/FTAs (`IpcChannelUnavailable`, `NotificationNotDelivered`,
`ServerMessageHandlingFailure`, `MessageTimingViolated`, `IpcApiMisuseOrLifecycleViolation`,
`ConnectionContextDataWrong`, `StateMachineError`) currently have **no** corresponding
`ControlMeasure` records. Flagged in `backlog.md`.

## Software architectural design overview

- `static_design.puml` — top-level `dependable_element_message_passing` SEooC box containing
  `component_message_passing` (units: `client_connection`, `server_connection`, and a `dispatch`
  sub-component with units `qnx_dispatch` and `unix_domain`), exposing a single public API port
  (`score::message_passing`) and consuming an `os` port.
- `public_api.puml` / `private_api.puml` — public vs. private interface surfaces.
- `client_connection_activity_diagram.puml`, `server_client_sequence.puml` — client connection
  lifecycle and client/server message-exchange sequencing.
- `client-server.md` — the prose design rationale (see below); by far the most detailed artifact
  in this component and the natural first read for any future actualization cycle. Covers: why the
  design moved away from POSIX mqueue and the old short/medium message split, the
  Server/ServerConnection/ClientConnection/ClientFactory/ServerFactory abstractions, the client
  and server state machines, the abstracted `SEND`/`REQUEST`/`REPLY`/`NOTIFY` wire protocol,
  client- and server-side implementation notes (thread models, QNX resource-manager questions,
  ring-buffer sizing), safety concerns for the four safe/QM client/server combinations, timing
  guarantees (explicitly *not* provided in general), and two concrete usage examples (DataRouter
  logging/tracing source, DataRouter subscriber).

## Current public API surface (headers in `score/message_passing/`)

- `i_client_connection.h` — `IClientConnection`: `Send`, `SendWaitReply`, `SendWithCallback`,
  `GetState`, `GetStopReason`, `Start`, `Stop`, `Restart`; `State` and `StopReason` enums;
  `ReplyCallback`, `NotifyCallback`, `StateCallback` typedefs.
- `i_client_factory.h` — `IClientFactory::Create(ServiceProtocolConfig, ClientConfig)`;
  `ClientConfig` (`max_async_replies`, `max_queued_sends`, `fully_ordered`, `truly_async`,
  `sync_first_connect`).
- `i_server.h` — `IServer::StartListening(ConnectCallback, DisconnectCallback, MessageCallback,
  MessageCallback)`, `StopListening()`.
- `i_server_factory.h` — `IServerFactory::Create(ServiceProtocolConfig, ServerConfig)`;
  `ServerConfig` (`max_queued_sends`, `pre_alloc_connections`, `max_queued_notifies`).
- `i_server_connection.h` — `IServerConnection`: `GetClientIdentity`, `GetUserData`, `Reply`,
  `Notify`, `RequestDisconnect`.
- `i_connection_handler.h` — `IConnectionHandler`: `OnMessageSent`, `OnMessageSentWithReply`,
  `OnDisconnect`.
- `service_protocol_config.h` — `ServiceProtocolConfig` (`identifier`, `max_send_size`,
  `max_reply_size`, `max_notify_size`), shared by both client and server factories.

## Changelog

(none yet — this section is appended to, never rewritten, per cycle that touches the narrative
above; see `rules-score-actualize` SKILL.md "Keeping it current".)
