# Message Passing — Change Request: feature-req-notify-split-and-crossplatform-qm

## Trigger

Two items deliberately deferred out of the closed `changes/2026-09-19-assumed-system-requirements-rewrite/`
cycle (see its `backlog.md` entries, carried forward from `evidence_bundle.md`), now picked up as
their own cycle at the human's request ("let's move to refining feature requirements"):

1. **No `FeatReq` distinguishes `Notify` from `Send`.** Both server-initiated notification
   (`IServerConnection::Notify`) and client-initiated one-way send (`IClientConnection::Send`)
   currently derive from the same assumed-system capability,
   `MessagePassing.OneWayMessageDeliveryCapability@1`, at the `FeatReq` layer too — there is only
   one `FeatReq` pair covering one-way delivery
   (`SynchronousUnidirectionalCommunication`/`AsynchronousUnidirectionalCommunication`), and
   `IServerConnectionNotifyAPI` (`CompReq`) derives straight from `ServerInterface@2` instead of
   from a `Notify`-specific `FeatReq`. The assumed-system layer intentionally merged the two
   capabilities (2026-09-19 decision); whether they warrant separate `FeatReq` records is a
   feature-layer question, not an assumed-system one.
2. **`MessagePassing.CrossPlatformAbstraction@1` (QM) has zero `FeatReq` children.** No feature
   requirement currently expresses "QM-quality implementations of the API are permitted on
   non-QNX/non-certified host OSes" — `OSIndependentAPI` derives from the ASIL B
   `QnxAsilBQualifiedImplementation@1` instead, and correctly so (its content is "the API contract
   itself is OS-independent", a different claim from "QM implementations are allowed off the
   certified target").

## Classification

Both are **"was right, world changed" in the loose sense of "not yet authored"**, not defects: the
2026-09-19 cycle explicitly scoped these out as separate feature-layer decisions rather than
folding them into the assumed-system rewrite (Core Principle 5 — the assumed-system merge/split
decisions were deliberate; whether the feature layer needs finer granularity is a distinct,
not-yet-made decision). Nothing existing is factually wrong; both are coverage gaps identified
during that cycle and consciously deferred.

## Stated scope

Per human confirmation (2026-09-22): tackle **both** backlog items in this cycle.

1. Decide whether `Notify` and `Send` warrant separate `FeatReq` records (splitting
   `AsynchronousUnidirectionalCommunication` and/or `SynchronousUnidirectionalCommunication`, or
   adding a new dedicated `FeatReq`), and re-point `IServerConnectionNotifyAPI` accordingly if a
   split happens.
2. Author a new `FeatReq` deriving from `CrossPlatformAbstraction@1` expressing the QM
   implementation-on-non-QNX-OS capability.

## Open Questions — RESOLVED 2026-09-22 (human answers)

1. **Notify vs Send are different features.** `Notify` (server→client) is fully asynchronous,
   unconditionally. Client `Send` is asymmetric/configuration-dependent: it is required to be
   queued (not sent inline) if `ClientConfig::truly_async` is true, or if a reply is currently
   pending on the connection (the "one other condition"); otherwise it is allowed to call the OS
   transport directly, which can block. The existing `CompReq.SynchronousSendBlocksUntilServerReceives`
   is wrong to bind this to "no client-side send queue is configured" — confirmed in
   `client_connection.cpp`: the direct/blocking path is selected by `!fully_ordered &&
   !truly_async`, independent of `max_queued_sends`. Additionally, "blocks until transferred and a
   suitable handler identified" holds for the QNX backend (synchronous IPC primitive) but not the
   Linux backend (`write()`/`send()` to a Unix Domain Socket returns once kernel-buffered, no
   receiver-side confirmation). An alternative design (model one-way `Send` as
   `SynchronousBidirectionalCommunication` with an empty reply, portable to both backends) was
   raised as a thought but is **not** directed to be implemented — the actual ask is: rewrite the
   `FeatReq` (and rippled `CompReq`) language to stop overclaiming a QNX-only property as a
   universal feature, and to reflect the real asymmetry between client `Send` and server `Notify`.
2. **Yes** — QM implementations on non-QNX OSes should be its own discoverable `FeatReq`, not
   something only visible by reading `component_requirements.trlc`/`external_component_requirements.trlc`.

See `impact_analysis.md` (Step 1) for the full upward/downward trace and the resulting three
remaining open questions (naming, whether to correct-in-place vs. replace the two wrong `CompReq`s,
and whether the new QM `FeatReq` should derive into `external_component_requirements.trlc`).
