# Message Passing — Change Request: public-api-diagram-requirements-review

## Trigger

The human rebased `dependability/software_architectural_design/public_api.puml` in three same-day
commits:

1. `304ac462` — `[message passing] update architecture` (baseline, predates this cycle).
2. `ba1e1d96` — `docs(message_passing): fix public_api.puml to match actual API` — replaced
   invented methods (`CreateClientConnection`, `SendWithReply`, `Set*Callback`) with the real
   `IClientConnection`/`IServer`/`IServerConnection`/`IClientFactory`/`IServerFactory`/
   `IConnectionHandler` interfaces and methods.
3. `d3b34c89` — `docs(message_passing): add ClientFactory/ServerFactory/Engine constructors` —
   added the concrete, platform-selected classes (`Engine`, `ClientFactory`, `ServerFactory`) that
   consumers actually instantiate (`client_factory.h`, `server_factory.h`, `engine.h`), including
   their constructors and `GetEngine()`/`GetDefaultOsResources()`.

The human then asked for a review: does the new `public_api.puml` make sense, and does the
combination of `FeatReq`/`public_api.puml`/`CompReq` have anything **obviously missing**? Called
out one concrete tricky case explicitly: `GetDefaultOsResources()` is shown on the `Engine` class,
but its only real use is supplying the default argument of an otherwise-mockable constructor
parameter (see `qnx_dispatch_engine.h`'s `OsResources`-accepting constructor) — is a dedicated
requirement for it warranted, and if so, what kind?

## Classification

**Coverage gap, not a defect in existing frozen content.** Nothing in `component_requirements.trlc`
is factually wrong given what it already claims to cover. The gap exists because `d3b34c89`
promoted `Engine`/`ClientFactory`/`ServerFactory` — previously undiagrammed implementation classes
— into the public API diagram for the first time; the `CompReq` layer has not yet caught up to
that promotion ("was right, world changed" per Core Principle 5 — the diagram's scope genuinely
grew, no one authored the wrong thing).

One item is a narrower, pre-existing, already-logged **defect**: the `bazel build` of
`message_passing_architectural_design` reports 2 validator errors because `static_design.puml`'s
top-level `message_passing_public_api`/`os` interfaces have no matching top-level declaration in
`public_api.puml`. This was already found and logged during the
`changes/2026-09-17-architecture-completeness-for-fta-redo/` cycle (see its `backlog.md` entry,
"From the 2026-09-17 architecture-completeness-for-fta-redo cycle") and remains unfixed — this
cycle reconfirms it (re-ran the build, same 2 errors) but does not re-log it or treat it as new.

## Stated scope

Review only, per the human's actual request — no artifact edit was asked for yet. Concretely:

1. Verify `public_api.puml`'s new content (method signatures, ctor signatures, class/interface
   relationships) against the real headers.
2. Cross-check `public_api.puml` against `feature_requirements.trlc`/`component_requirements.trlc`
   for gaps in either direction (diagram element with no requirement, or vice versa).
3. Specifically answer: does `GetDefaultOsResources()` need its own `CompReq`?

No decision has yet been made on whether/which candidate gaps below should be turned into actual
new `CompReq` records (that is Step 2+ of this lifecycle, gated on the checkpoint below).

## Open Questions

1. **RESOLVED 2026-09-24 (human):** yes — new `CompReq`s `ClientFactoryEngineSharingAPI`/
   `ServerFactoryEngineSharingAPI`, `derived_from = [MessagePassing.SingletonFreeImplementation@2]`
   only (not `AllowsBoundedMonotonicMemoryAllocation`).
2. **RESOLVED 2026-09-24 (human):** no `CompReq` — `QnxDispatchEngine`'s `OsResources`-accepting
   constructor is not part of the public API (`public_api.puml`'s `Engine` class only shows
   `Engine(memory_resource, logger)`; that constructor is QNX-internal/test-only), so it does not
   need component-requirement coverage the way diagrammed public API surface does.
3. **RESOLVED 2026-09-24 (human):** yes — four new `CompReq`s added:
   `IClientConnectionStartAPI`, `IClientConnectionStopAPI`, `IClientConnectionRestartAPI`,
   `IClientConnectionGetStopReasonAPI`, all `derived_from = [MessagePassing.OSIndependentAPI@2]`,
   matching `IClientConnectionGetStateAPI`'s existing parent choice.
4. **RESOLVED 2026-09-24 (human):** defer to a later cycle — not folded into this one. Stays as the
   existing `backlog.md` entry (from `changes/2026-09-17-architecture-completeness-for-fta-redo/`).

See `impact_analysis.md` for the upward/downward trace behind each of these.
