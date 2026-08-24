# Service Discovery — Problem Statement

> Step 0 draft per the `rules-score` skill. This is a **fresh** problem statement for a new,
> standalone SEooC at `score/service_discovery/` (not under `score/mw/com/`). It is derived from
> the informal input and from reading existing code as behavioural evidence — it does **not**
> reuse requirement wording, names, or layering from the PoC at `score/mw/com/service_discovery/`,
> which is treated as discardable per the "Handling a rework" section of `rules-score`.

## Terminology

- **SD (Service Discovery)**: the new component being specified here — a central `Daemon` process
  plus the client library applications use to talk to it. Distinct from the current
  `score::mw::com` abstraction of the same name (`impl/i_service_discovery.h`), which is a
  binding-independent façade that a LoLa binding implements on top of *this* SD.
- **Daemon**: the single, central, authoritative process holding the registry of all currently
  registered service instances on the ECU/host. Exactly one authoritative daemon instance is
  assumed (mirrors the PoC's `SingleDaemonAuthority` assumption of use, re-derived independently
  here rather than copied).
- **Service-instance**: one concrete, addressable instance of a service-type (in LoLa terms: a
  `(service_id, instance_id)` pair). A **service-type** identifies the interface/contract; a
  service-instance identifies one deployed occurrence of it.
- **Register / Unregister**: RPC-shaped operations by which a *provider* process tells the daemon a
  service-instance is now offered / no longer offered.
- **FindService**: a one-shot, synchronous RPC lookup for service-instances of a given type,
  either a specific instance-id or "any instance".
- **StartFindService / StopFindService**: RPC pair that registers/cancels a standing search; the
  daemon pushes asynchronous notifications for that search until it is stopped or the caller
  disconnects. The `StartFindService` reply carries both the current snapshot and a **handle**
  used later to `StopFindService`.
- **Creation lock (exclusive)**: a mutex-like, per-service-instance lock a provider must hold
  before creating the instance's backing resources (e.g. shared-memory objects), replacing the
  informal input's `ServiceExistanceMarkerFile` (flock-based today, see
  [score/mw/com/impl/bindings/lola/partial_restart_path_builder.cpp](../../mw/com/impl/bindings/lola/partial_restart_path_builder.cpp)
  and [score/mw/com/impl/bindings/lola/skeleton.h](../../mw/com/impl/bindings/lola/skeleton.h)).
  Only one holder at a time; the daemon — not the filesystem — is authoritative.
- **Usage lock (shared/exclusive)**: a shared-mutex-like, per-service-instance lock consumers hold
  (shared) while using an instance's resources and providers can hold (exclusive) to gate against
  active consumers, replacing the informal input's `ServiceUsageMarkerFile` (flock-based today,
  used from [score/mw/com/impl/bindings/lola/proxy.h](../../mw/com/impl/bindings/lola/proxy.h)).
- **Session**: the daemon-side notion of one connected client (provider or consumer), tied 1:1 to
  one message-passing connection's lifetime. All locks/registrations/subscriptions a session owns
  are released automatically when the daemon observes that connection close (crash or graceful
  shutdown) — the daemon relies on message-passing connection-loss detection as its sole liveness
  signal, it does not do its own heartbeating.
- **Message-passing**: the existing same-host IPC abstraction in `score/message_passing/`
  (`IClientConnection`/`IServer`, Unix domain sockets on Linux, native dispatch messaging on QNX).
  SD is a client and server built entirely on top of this abstraction; it does not open its own
  transport.
- **RPC semantic** (this document): realized via `message_passing::IClientConnection::SendWaitReply`
  — blocking request/reply on the caller's thread, bounded by a fixed `max_send_size`/
  `max_reply_size` agreed at connection-setup time (see Expectations Toward the Environment).
- **Notification semantic** (this document): realized via `message_passing`'s server-to-client
  `Notify` push mechanism — non-blocking, best-effort-queued, bounded by a fixed `max_notify_size`.
- **Batching**: collecting multiple logical daemon operations (e.g. several `Register` calls, or a
  `StartFindService` covering several service-types) into a single message-passing round-trip, to
  avoid one round-trip per call during ECU startup storms. Transparency to the LoLa caller is the
  stated goal in the informal input, not a settled design.
- **UID-based static authorization**: a startup-loaded policy mapping which OS user id (UID) may
  register which service-instance, enforced by the daemon as a second line of defense on top of
  the existing shared-memory/UID/ACL mechanism in `score/mw/com`.

## System Slice

This SEooC is a **new, standalone component at `score/service_discovery/`** — it is not nested
under `score/mw/com/` even though `score::mw::com` (LoLa) is (today) its only known integrator.
It consists of:

- A **daemon** process: owns the central in-memory registry of service-instance registrations,
  the creation/usage locks, and the set of active `StartFindService` subscriptions; talks to
  clients only via `score/message_passing/`.
- A **client library** linked into application processes (providers and consumers) that wraps the
  message-passing protocol calls (`Register`, `Unregister`, `FindService`, `StartFindService`,
  `StopFindService`, lock acquire/release) behind a typed API.

**Depends on:** `score/message_passing/` — specifically `IClientConnection::SendWaitReply` for all
RPC-shaped calls listed above, and the server-side `Notify` mechanism for
`StartFindService`-driven async updates. Both Linux (Unix domain sockets) and QNX (native dispatch)
message-passing backends must be supported through the same daemon/client protocol, since SD itself
does not distinguish OS.

**Replaces / extracts evidence from:**
- `score/mw/com/service_discovery/` — a PoC daemon/client/registry/protocol implementation that
  already covers most of the RPC surface above (`Register`, `Unregister`, `Resolve`≈`FindService`,
  `StartFindService`/`StopFindService`, creation/usage locks, UID/PID capture via
  `IServerConnection::GetClientIdentity()`) and demonstrates that a message-passing-based daemon
  design works end-to-end. Its `dependability/` TRLC/PlantUML content is semantically wrong for
  this new component's purposes and must be re-derived, not copied; its code may inform (but not
  dictate) protocol/state-machine design during Step 3 (architecture).
- `score/mw/com/impl/bindings/lola/` marker-file mechanism — the *actual current* implementation of
  what the informal input calls `ServiceExistanceMarkerFile`/`ServiceUsageMarkerFile` lives here,
  not in `service_discovery/`: flock-based exclusive/shared locks
  (`score::memory::shared::ExclusiveFlockMutex`/`SharedFlockMutex`) on marker files under
  `/tmp/mw_com_lola/partial_restart/...`, built in
  [partial_restart_path_builder.cpp](../../mw/com/impl/bindings/lola/partial_restart_path_builder.cpp),
  [skeleton.h](../../mw/com/impl/bindings/lola/skeleton.h)/[skeleton.cpp](../../mw/com/impl/bindings/lola/skeleton.cpp)
  (existence marker, exclusive), and
  [proxy.h](../../mw/com/impl/bindings/lola/proxy.h) (usage marker, shared/exclusive), with
  discovery driven by inotify watches in
  [impl/bindings/lola/service_discovery/client/service_discovery_client.h](../../mw/com/impl/bindings/lola/service_discovery/client/service_discovery_client.h).
  These are the mechanisms the new daemon must absorb (see Informal Functional Requirements below);
  this SEooC replaces the flock/inotify/filesystem implementation, not just the SD daemon PoC.

**Integrator:** `score/mw/com/` (LoLa). Today it reaches SD-like behaviour only through the marker
files above and through the PoC's `service_discovery/` daemon (used by
`score/mw/com/service_discovery/service_discovery_compat.h`, which currently adapts the PoC daemon
to LoLa's existing `impl::IServiceDiscovery`/`impl::IServiceDiscoveryClient` interfaces). Whether the
LoLa-specific adaptation layer (`service_discovery_compat.*` and equivalent glue) should live inside
this new SEooC or purely in `score/mw/com/` is an open question, tracked in `backlog.md`, not
resolved here.

## Informal Functional Requirements

Distilled from `research/inputs/input_for_new_service_discovery_implementation.md` (verbatim
rendering of the source `.docx`); numbering is for traceability only, not final requirement IDs.

1. The daemon is a single central process holding the authoritative registry of all currently
   registered service-instances.
2. All communication between SD clients and the daemon uses the existing `message_passing`
   abstraction (Linux and QNX implementations already exist and must both be supported).
3. `Register`: a provider registers a new service-instance. RPC semantic (`SendWaitReply`).
4. `Unregister`: a provider unregisters a previously registered service-instance. RPC semantic.
5. `FindService`: a one-shot synchronous lookup for service-instances of a given service-type,
   either a specific instance-id or "any instance". RPC semantic.
6. `StartFindService`: registers a standing search for a given service-instance (specific or
   any-instance). RPC semantic. The RPC reply contains (a) the current snapshot of matching
   service-instances and (b) a handle to later stop the search.
7. After `StartFindService`, the daemon pushes notifications (via `message_passing`'s notification
   mechanism) to the caller whenever matching service-instance availability changes, until the
   search is stopped or the connection is lost.
8. `StopFindService`: stops a search previously started via `StartFindService`, using the handle
   from its reply. RPC semantic.
9. An `score::mw::com` application connects at message-passing level to the daemon at startup and
   disconnects at shutdown; the daemon must interpret any connection loss as that application
   having terminated (crash or graceful shutdown), and clean up all state that application owned.
10. The daemon shall absorb the current `ServiceExistanceMarkerFile` mechanism: an exclusive
    "creation lock" per service-instance that a provider must acquire before creating shared-memory
    objects and registering. Acquisition fails if another session already holds it.
11. The daemon shall absorb the current `ServiceUsageMarkerFile` mechanism: a shared/exclusive
    "usage lock" per service-instance. Providers can take it exclusively; consumers take it shared.
    Both providers and consumers get APIs for their respective lock modes.
12. Because the daemon now owns both the registration and the creation lock, it shall:
    a. automatically release the creation lock when the owning provider unregisters the instance;
    b. automatically release the creation lock **and** unregister the instance when the daemon
       detects the provider's connection has been lost.
13. The daemon shall be able to automatically release usage locks based on message-passing
    connection state (i.e. when the lock-holding session disconnects), without a separate
    unlock call being required.
14. During ECU startup, many providers and consumers interact with the daemon in a short window;
    each interaction is a round-trip. A batching mechanism is needed so a set of API calls can be
    collected and sent as a single message-passing round-trip, returning a set of per-call results.
    Whether batching is transparent to the LoLa-level caller or explicit in the SD API is to be
    analyzed; transparent batching is the stated preference, not a settled decision.
15. The daemon shall support a static, startup-loaded authorization policy (informed by the LoLa
    configuration model) mapping which UID(s) may offer which service-instance, and shall deny
    `Register` from an unauthorized UID. This is a second line of defense on top of the existing
    shared-memory/UID/ACL mechanism in `score/mw/com`, not a replacement for it.

## Expectations Toward the Environment

These seed `AssumedSystemReq`/`AoU` authoring in Step 1 — they are not yet TRLC.

- The host OS provides the `score/message_passing/` transport (Unix domain sockets on Linux, native
  dispatch messaging on QNX); SD does not implement its own IPC transport and does not cross the
  network stack.
- Per `service_protocol_config.h`, `max_send_size` / `max_reply_size` / `max_notify_size` are each
  fixed once, at connection/protocol setup time, **per protocol message kind** — not renegotiable
  per message. Any message exceeding its configured limit is rejected by the transport
  (`EMSGSIZE`) rather than truncated or fragmented automatically. SD's protocol design must pick
  fixed sizes large enough for any single atomically meaningful request, and must define its own
  application-level strategy (not provided by `message_passing`) for anything that does not fit
  in one message — e.g. a batched or list-style request such as "subscribe to N service-types in
  one call". See Open Questions.
- The OS provides trustworthy per-connection identity (UID, PID) that message-passing surfaces via
  `IServerConnection::GetClientIdentity()`; the daemon relies on this, not on payload-asserted
  identity, for ownership checks and the static UID authorization policy.
- Exactly one daemon instance is the registration authority for the host/ECU at any time; SD does
  not need to support multiple cooperating daemons or leader election.
- The daemon is assumed to start before any of its clients attempt to connect; a client's initial
  connection attempt during the ECU startup window may need to tolerate the daemon not yet being
  up (retry/backoff), but that policy lives in the client, not as a daemon assumption.
- Message-passing connection loss is assumed to be a reliable, timely signal of client-process
  termination (crash or graceful exit) — SD's cleanup-on-disconnect behavior (locks, registrations,
  subscriptions) depends on this being true and prompt enough to bound how long a stale
  registration can survive its owning process.
- A monotonic time source is assumed available if any lease/staleness-supervision behavior is
  adopted (as hinted by the PoC's now-discarded `StaleEntrySupervision` idea) — not yet decided for
  this component; carried here only as an environmental capability to note, not a commitment.

## Open Questions

1. **Batch message sizing strategy.** `message_passing` requires fixed max sizes per message kind
   set in advance. A group operation (e.g. subscribing to a list of service-types in one call, or a
   batch of `Register`/lock calls during startup) may not fit in one message. Options to evaluate
   in Step 1/2: (a) split into multiple message-passing messages per logical batch, transparently
   reassembled at the protocol layer; (b) a bounded max batch size chosen so the worst case still
   fits one message, with callers required to split larger requests themselves; (c) some hybrid.
   This must be resolved before `max_send_size`/`max_reply_size` values can be chosen — do not size
   these by guesswork.
2. **Should batching be transparent to the LoLa API caller, or explicit in the SD client API?** The
   informal input leans transparent; needs a human decision once the architecture step has a
   concrete protocol shape to evaluate against.
3. **Where should the LoLa-specific integration/adaptation layer live** — inside
   `score/service_discovery/` (as the PoC's `service_discovery_compat.*` does today) or entirely
   inside `score/mw/com/`? A SEooC "out of context" arguably should not contain `mw/com`-specific
   glue. Not resolved here; tracked in `backlog.md`.
4. **ASIL classification.** The PoC uses `ASIL.B` throughout its (to-be-discarded) TRLC. Treated
   here only as a starting hypothesis to confirm or revise during Step 1 (assumed-system
   requirements), not as settled fact for this component.
5. **Relationship to the existing marker-file mechanism during migration.** The informal input
   describes the daemon absorbing creation/usage locking outright; it does not address whether
   `score/mw/com`'s current flock/inotify-based marker files must be supported in parallel during a
   transition period, or can be removed atomically once SD is available. Needs a human decision
   before Step 3 (architecture) fixes the daemon's lock-state model.
6. **Liveness/staleness supervision.** Is a lease-based or otherwise time-bounded staleness check
   needed in addition to connection-loss-triggered cleanup (the PoC had a now-discarded
   `StaleEntrySupervision` idea), or is connection-loss detection alone considered sufficient given
   the "Expectations Toward the Environment" above? Needs a human decision before Step 1.
7. **Authorization policy format and delivery.** The informal input says "provide this static
   information to the daemon at startup" but does not specify the policy's format/source (e.g.
   generated from the same LoLa configuration model, a separate file, command-line argument). Needs
   clarification before this becomes a testable requirement in Step 2.
</content>
