# Message Passing — Impact Analysis: feature-req-notify-split-and-crossplatform-qm

## Upward trace (root-cause localization)

### Thread 1: Notify vs Send

`IServerConnectionNotifyAPI` (`CompReq`) derives from `ServerInterface@2` (`FeatReq`: "registers
connection handlers and processes incoming requests") — a mismatch even before considering
Notify's asynchrony: `ServerInterface` is about the server *receiving/dispatching*, while `Notify`
is the server *sending*. There is no `FeatReq` at all for "server sends a one-way notification to
the client." The true origin is a **missing `FeatReq`**, not a wrong pin.

Tracing further, the existing one-way `FeatReq`s
(`SynchronousUnidirectionalCommunication`/`AsynchronousUnidirectionalCommunication`) are written
exclusively from the client's `Send()` point of view (`i_client_connection.h`) and both derive from
`MessagePassing.OneWayMessageDeliveryCapability@1`. `Notify` (`IServerConnection::Notify`,
`i_server_connection.h`, server→client direction) is a structurally different capability: per human
confirmation, it is **unconditionally asynchronous** (confirmed in code —
`UnixDomainServer::ServerConnection::Notify` always calls `SendProtocolMessage` directly with no
config branch; `QnxDispatchServer::ServerConnection::Notify` queues via a
`ServerConfig::max_queued_notifies`-sized pool), whereas client `Send` is asymmetric/configurable
(see Thread 2). `OneWayMessageDeliveryCapability@1` (`AssumedSystemReq`) already covers "a message
delivered either direction without a reply" at the system level, so a **new `FeatReq`** for
server-initiated notification, deriving from the same `AssumedSystemReq`, is the correct fix — not
a re-pin of an existing record.

### Thread 2: `SynchronousUnidirectionalCommunication` wording defect

`FeatReq.SynchronousUnidirectionalCommunication@2` states: *"The send call blocks until the message
has been transferred to the receiving side and a suitable handler has been identified."* This
claim is asserted as a universal feature-level guarantee. Reading `ClientConnection::Send`
(`client_connection.cpp`) shows this is not what the code guarantees:

- `Send()` calls `engine_->SendProtocolMessage(...)` **directly** (the actually-blocking-on-OS-call
  path) whenever `!fully_ordered && !truly_async` — **regardless of `max_queued_sends`**.
- When `fully_ordered || truly_async`, the call queues instead of calling `SendProtocolMessage`
  directly only if (a) `truly_async` is set, **or** (b) a `SendWaitReply`/`SendWithCallback` reply
  is currently pending on the same connection (`waiting_for_reply_.has_value()`) — the second,
  currently-undocumented condition the human pointed at ("required to be queued ... in one other
  condition").
- Even in the direct (`SendProtocolMessage`) path, "blocks until transferred ... and a suitable
  handler has been identified" is a QNX-specific property (QNX's native IPC primitive is
  synchronous end-to-end). The Linux backend's `SendProtocolMessage` is a `write()`/`send()` to a
  Unix Domain Socket, which returns once the kernel has buffered the data — it does not wait for
  the server process to read or dispatch to a handler.

So the FeatReq's wording is **wrong today** (Core Principle 5): it generalizes a QNX
implementation detail into a cross-platform feature guarantee that the Linux backend does not
provide, and it does not mention the actual queuing trigger conditions at all. This is the true
root of the currently-wrong `CompReq.SynchronousSendBlocksUntilServerReceives` ("...when no
client-side send queue is configured" — also wrong, since the direct-call path is selected by
`!fully_ordered && !truly_async`, not by whether a queue is configured) and of
`CompReq.AsynchronousSendReturnsAfterLocalAcceptance` ("...when a client-side send queue is
configured" — same defect, inverted).

The human noted an alternative design framing (model one-way `Send` as
`SynchronousBidirectionalCommunication` with an empty reply, which would be portable to both
backends) but did not direct that the API/architecture be changed — only that the **requirement
language** be corrected to stop overclaiming a QNX-only property as a general feature, and to
reflect the real, configuration-dependent, asymmetric client/server behavior. No architecture or
code change is in scope for this cycle.

### Thread 3: `CrossPlatformAbstraction` has no `FeatReq` child

Confirmed in the 2026-09-19 cycle's evidence: `CrossPlatformAbstraction@1` (QM) has zero `FeatReq`
children today. Per human confirmation this cycle, "Linux support (as QM)" should be directly
discoverable as a `FeatReq`, not left implicit in `external_component_requirements.trlc`'s
`TransportMechanismOnLinux` (which is also not wired into any Bazel target — see `backlog.md`).

## Downward trace (ripple set)

| Reference | Current | Must become |
|---|---|---|
| `CompReq.IServerConnectionNotifyAPI.derived_from` | `[ServerInterface@2]` | new `FeatReq` for server notification (name TBD) |
| `CompReq.SynchronousSendBlocksUntilServerReceives.derived_from` | `[SynchronousUnidirectionalCommunication@2]` | same `FeatReq`, new version (content of both is fixed together) |
| `CompReq.SynchronousSendBlocksUntilServerReceives.description` | "...when no client-side send queue is configured" | corrected wording matching the real `truly_async`/pending-reply condition, or retire+replace if the FeatReq split makes it obsolete |
| `CompReq.AsynchronousSendReturnsAfterLocalAcceptance.derived_from` | `[AsynchronousUnidirectionalCommunication@2]` | same `FeatReq`, new version |
| `CompReq.AsynchronousSendReturnsAfterLocalAcceptance.description` | "...when a client-side send queue is configured" | corrected wording, same condition fix |
| `CompReq.IClientConnectionSendAPI.derived_from` | `[SynchronousUnidirectionalCommunication@2]` | same `FeatReq`, new version (pure re-pin, description itself ("provides a Send method...") is still accurate) |
| No existing reference | — | new `CompReq`(s) needed for the new "Linux/QM" `FeatReq`? Or does `TransportMechanismOnLinux` (`external_component_requirements.trlc`) already cover this at the wrong layer? — open question, see below |

No FTA `$BasicEvent` aliases, `lobster-tracing` ids, or `test_case_coverage.lock.yaml` entries
reference any of `SynchronousUnidirectionalCommunication`, `AsynchronousUnidirectionalCommunication`,
`IServerConnectionNotifyAPI`, or `CrossPlatformAbstraction` directly (verified: these identifiers do
not appear in `safety_analysis/*.trlc` or `*.puml`) — the ripple set is closed at the
`requirements/` layer.

## Artifacts to touch

- `requirements/feature_requirements.trlc`:
  - Rewrite `SynchronousUnidirectionalCommunication` (bump `@2`→`@3`): keep it scoped to *client*
    one-way `Send`, describe the real configuration-dependent blocking/queuing behavior, and stop
    asserting the QNX-only "handler identified" guarantee as universal.
  - Rewrite `AsynchronousUnidirectionalCommunication` (bump `@2`→`@3`) to match, for consistency
    (both describe the same underlying `Send()` call from different configuration angles).
  - Add a new `FeatReq` for server-initiated `Notify` (deriving from
    `OneWayMessageDeliveryCapability@1`, ASIL B, unconditionally-asynchronous wording).
  - Add a new `FeatReq` for QM implementations on non-QNX OSes (deriving from
    `CrossPlatformAbstraction@1`, QM).
- `requirements/component_requirements.trlc`:
  - Re-pin `IClientConnectionSendAPI`, `SynchronousSendBlocksUntilServerReceives`,
    `AsynchronousSendReturnsAfterLocalAcceptance` to the bumped `FeatReq` versions.
  - Correct `SynchronousSendBlocksUntilServerReceives`/`AsynchronousSendReturnsAfterLocalAcceptance`
    wording to match the real `truly_async`/pending-reply condition (bump their own `version`, since
    their content — not just their pin — is wrong today).
  - Re-point `IServerConnectionNotifyAPI` to the new Notify `FeatReq` (bump its own `version`, since
    its `derived_from` target identity changes).

## Artifacts explicitly NOT touched (and why)

- `assumed_system/assumed_system_requirements.trlc` — `OneWayMessageDeliveryCapability` and
  `CrossPlatformAbstraction` already correctly cover both directions/both platform tiers at the
  system level; the gap is purely that the `FeatReq` layer under them is incomplete/wrong, not that
  the assumed-system layer itself needs to change.
- `i_client_connection.h`/`i_server_connection.h`/`client_connection.cpp` and other implementation
  code — this cycle corrects requirement *wording* to match already-existing, already-tested
  behavior; no code or API change is implied or in scope.
- `safety_analysis/*.trlc`, `fta_*.puml` — confirmed no alias references the touched identifiers;
  out of scope per the closed ripple set above.
- `external_component_requirements.trlc` (`TransportMechanismOnLinux`) — left as-is pending the open
  question below; not editing until it's decided whether the new "QM on non-QNX" `FeatReq` should
  derive down into it or stay a leaf.

## Open questions before Step 2 edits

1. Naming for the two new `FeatReq` records (proposed: `AsynchronousServerNotification` for
   `Notify`, `QMImplementationOnNonQnxOS` for the Linux/QM capability) — confirm or rename.
2. Should `CompReq.SynchronousSendBlocksUntilServerReceives`/
   `AsynchronousSendReturnsAfterLocalAcceptance` be corrected in place (same identifiers, fixed
   wording + version bump), or retired and replaced by a single new `CompReq` that describes the
   real `truly_async`-and-pending-reply-driven condition without implying two cleanly-separate
   "modes"? Corrected-in-place is more minimal (Core Principle 1); replacement is more honest that
   the old framing ("configured queue" as the discriminator) was itself the defect.
3. Should the new "QM on non-QNX OSes" `FeatReq` derive down into anything in
   `external_component_requirements.trlc`, or remain a leaf for now (that file isn't Bazel-wired
   yet — see `backlog.md`)?
