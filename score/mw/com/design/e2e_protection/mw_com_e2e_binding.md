# E2E Responsibility Allocation in `mw::com`

## Status

**Proposed**

## Motivation

End-to-End (E2E) protection consists of two conceptually different responsibilities:

1. **Generation and verification of E2E protection information** (counter, CRC, Data ID handling)
2. **Interpretation of E2E check results over time** (historical health tracking and status evaluation)

To maintain the `mw::com` architecture principle of **binding-independent communication management** with **binding-specific transport implementations**, these responsibilities must be clearly separated between the binding layer and the binding-independent implementation layer.

This document defines the allocation of E2E responsibilities, the interface contract between layers, and the implications for testing and safety classification.

---

## Architectural Principles

The existing `mw::com` architecture separates:

- **Binding-independent logic** (`impl/`, proxy/skeleton base classes, communication management)
- **Binding-specific logic** (LoLa today, future SOME/IP binding)

E2E integration follows the same principle:

```text
Application
    │
    ▼
Communication Management (binding-independent)
    │
    ▼
E2E Supervisor / Historical Health Tracker (binding-independent)  <- interprets categorical results over time
    │
    ▼
Binding Interface Contract                                <- always: categorical check results; optionally: header-stripped payload (never the E2E header / protected-data span)
    │
    ▼
LoLa Binding / SOME/IP Binding (binding-dependent)        <- owns ProtectMessage()/CheckMessage(), counter & CRC state
    │
    ▼
Transport
```

The binding layer understands the wire format of a transport and owns the profile-level Protect/Check algorithm and its state, while the binding-independent layer interprets the *sequence* of categorical results the binding produces over time — it does not compute them.

Two terms are used consistently below:

- **Check material** — the E2E header and the CRC-protected data span. Consumed only by `ProtectMessage()`/`CheckMessage()`; never leaves the binding.
- **Payload** (`raw_sample`, see [Proposed Contract](#proposed-contract)) — the header-stripped bytes the binding can hand up to the frontend alongside the categorical check results. The check results are always produced; whether the payload is consumed at all, and whether it is deserialized or passed through as raw bytes, is the frontend's/application's decision (driven by the payload type declared in the service interface), not the binding's.

---

## Responsibility Allocation

### Applicability Across Communication Element Types

E2E protection is defined per communication endpoint (event, field, and method request/response), not only for events. To remain binding-agnostic and avoid tying the E2E Manager to `SkeletonEventBase`/`ProxyEventBase` specifically, the responsibility allocation in this document generalizes to all `mw::com` communication element types:

```text
Binding (owns Protect/Check + their state)
        │  DataIntegrityStatus + SequenceStatus
        ▼
E2E Supervisor (binding-independent historical health tracker)
        │  E2EResult
        ▼
ProxyEventBase   ─┐
ProxyFieldBase    ├──►  (consuming element bases)
ProxyMethodBase  ─┘
```

and symmetrically on the provider side (`SkeletonEventBase` / `SkeletonFieldBase` / `SkeletonMethodBase`), whose bindings own the transmit-side counterpart — for events and fields, that is the whole picture, since the provider only ever protects and the consumer only ever checks; methods are the one exception (see below). The Protect/Check functions (`ProtectMessage`/`CheckMessage`) and their contexts generalize cleanly to all three element types — they operate per transmitted/received frame regardless of whether that frame carries a cyclic event, a field update, or a method request/response, and in every case live in the binding (`SkeletonEventBinding`/`ProxyEventBinding` and their Field/Method counterparts), not in the `*Base` classes.

> **Historical health tracking applicability — resolved.** The health counter is a pure function of the sequence of per-message check results fed into it (see [Binding Independence Assessment](#binding-independence-assessment-review-feedback), point 1) — it requires a *sequence of Check() results*, not a wall-clock cycle. For methods, that sequence is naturally "one result per completed call" (response received, client side; request received, server side) instead of "one result per reception cycle," so health tracking is meaningful for `ProxyMethodBase`/`SkeletonMethodBase` too. The one real difference from the event path: methods have no "no new data" branch, since a call is either pending or completed — there is no "missed cycle" concept the way there is for `GetNewSamples()` on events.
>
> **Method-specific profile variants.** Request/Response calls use method-specific profile variants that key Protect/Check off the request's session/client-server-transaction handle instead of a free-running cyclic counter. Historical health tracking runs on top of that per-call result exactly as it does for events, tracking degradation across a sequence of calls rather than across a sequence of reception cycles.

> **Note — binding maturity.** LoLa's `ProxyMethod`/`SkeletonMethod` support exists in the codebase (`impl/bindings/lola/proxy_method.h`, `skeleton_method.h`) but its public API is documented as still under development. This document treats the responsibility allocation as binding-agnostic; closing LoLa-specific method-support gaps is tracked separately with the team and is out of scope here.

> **Protect/Check ownership is bidirectional for methods.** Events and fields have a fixed direction — the provider (skeleton) only ever protects, the consumer (proxy) only ever checks. A method call has no such fixed direction: each side plays both roles depending on which leg of the call it's handling. `SkeletonMethodBinding` owns an `E2ECheckContext` for the incoming request — a skeleton that receives a call with In-Args must run `CheckMessage()` itself to decide whether the request is valid enough to process, exactly as a `ProxyEvent` does for received samples — in addition to the `E2EProtectContext` it already owns for the outgoing response. Symmetrically, `ProxyMethodBinding` owns an `E2EProtectContext` for the outgoing request in addition to the `E2ECheckContext` it already owns for the incoming response. Neither side is exclusively a protector or a checker the way `SkeletonEventBinding`/`ProxyEventBinding` are.

---

### Binding Layer Responsibilities

The binding layer is responsible for **protocol-specific placement and extraction of E2E information** and for invoking the E2E protection/check algorithm on the – possibly serialized – representation used by that transport: for a wire-serializing binding (SOME/IP) that means an actual byte buffer, while for LoLa it means the shared-memory slot and its parallel header array (see [Where the Header Bytes Actually Live](#where-the-header-bytes-actually-live-transmit-path-api-shape)) — there is no serialization step at all.

#### Transmit Path

The binding layer shall:

- Resolve, from binding-independent deployment configuration, whether the message/event is E2E-protected — this is a configuration decision, not one the binding makes on its own; the binding merely acts on it
- Locate the E2E header location in the transport representation
- Invoke the E2E protection algorithm
- Write/update E2E header fields in the outgoing sample/message
- Forward the protected message to the transport

#### Receive Path

The binding layer shall:

- Parse the transport-specific message format
- Locate the E2E header bytes and the protected-data view within the parsed frame
- Invoke `CheckMessage()` on that material, using a check state (`E2ECheckContext`) it owns per subscribing consumer, only when a new sample is actually present (see [Historical Health Tracking](#historical-health-tracking))
- Deliver the resulting raw sample (see [Proposed Contract](#proposed-contract)'s `BindingReceivedFrame`) together with the categorical `DataIntegrityStatus`/`SequenceStatus` pair to the binding-independent layer

The binding layer owns `CheckState` and invokes `CheckMessage()` directly — see [Receiver-Side Check State](#receiver-side-check-state) for the full rationale and a commercial-vendor precedent built exactly this way.

#### Binding Output

The binding layer **does not make communication-state decisions** — it never runs the historical health tracker and never sees its threshold configuration. What it produces, per received frame, is a small set of categorical results plus the (possibly null) raw sample — see [Proposed Contract](#proposed-contract).

The binding owns the profile check state and invokes `CheckMessage()` itself (see [Receiver-Side Check State](#receiver-side-check-state)), reusing a shared CRC/counter algorithm library. Two independent per-message results come out of that call — a data-integrity check (CRC/Data ID) and a sequence check (counter) — kept as separate categorical values rather than merged into one, since a CRC failure and a counter jump are actionable differently by a consumer (masquerading/corruption vs. loss/reorder):

```cpp
enum class DataIntegrityStatus {
    kDisabled, // Check was disabled
    kOk,       // Metadata and CRC match expectations
    kError,    // Metadata or CRC differ expectations
};

enum class SequenceStatus {
    kDisabled,                 // Check was disabled
    kOk,                       // No message lost between previous and current message (or this is the first message received)
    kOkGapWithinThreshold,     // Gap between the last and current message is within the allowed threshold
    kErrorRepeated,            // Message is a repetition of the last received message
    kErrorGapExceedsThreshold, // Gap between the last and current message exceeds the allowed threshold
};
```

> **Note:** the underlying profile-check algorithm computes CRC and counter validity as part of one call; this contract exposes the two outcomes separately rather than collapsing them into one merged error value, since data corruption/masquerading (`DataIntegrityStatus`) and loss/reorder (`SequenceStatus`) warrant different application-level handling.

Internally, the profile check also computes the actual counter delta before collapsing it into `SequenceStatus`. That delta is not part of the app-facing contract, but the binding surfaces it in an internal companion structure alongside the categorical results, so the binding-independent layer can reuse it for the per-consumer `MaxDeltaCounter` sizing guidance in [Distinguishing Intentional Under-Sampling from Genuine Loss](#distinguishing-intentional-under-sampling-from-genuine-loss):

```cpp
// Produced by the binding's own per-message check; consumed by the binding-independent layer's
// health tracking and MaxDeltaCounter sizing guidance — never exposed to applications
struct RawCheckOutcome {
    DataIntegrityStatus data_integrity;
    SequenceStatus sequence;
    std::uint16_t observed_delta;  // actual counter delta since the last processed sample
};
```

The binding is **not** stateless on the receive path — it owns the profile-level check state (`E2ECheckContext`) because that state (last accepted counter, CRC library context) is meaningless without the wire-format knowledge the binding already has. What stays binding-independent is the **interpretation** of the resulting sequence of categorical results over time — historical health tracking (`E2EHealthContext`) — which has no wire-format dependency at all (see [Receiver-Side Check State](#receiver-side-check-state) and [Sender-Side Protect State](#sender-side-protect-state) for the symmetric transmit-path treatment).

---

### Sender-Side Protect State

The E2E protect function `ProtectMessage()` requires a **mutable protect state** (`ProtectState`) that holds the current counter value and is updated on every call. Ownership of this state must be assigned to exactly one layer.

#### Options for `mw::com`

**Option A — Protect state in the binding-independent layer (`SkeletonEventBase`)**

An `E2EProtectContext` object (holding the counter) is owned by `SkeletonEventBase` alongside the existing binding pointer. Before calling `SkeletonEventBinding::Send()`, the binding-independent layer invokes `ProtectMessage()` to increment the counter and produce the E2E header bytes, then passes header bytes to the binding for injection.

```text
SkeletonEventBase (owns E2EProtectContext)         <- counter lives here
    │  calls ProtectMessage() → header bytes
    ▼
SkeletonEventBinding::Send(sample, e2e_header)     <- binding injects header at wire offset
```

*Pros:* Counter state is co-located with communication management. Independently testable without a transport. Matches the transmit topology: one event stream can be delivered to many consumers (multicast/pub-sub), so a single counter shared per `SkeletonEvent` is correct — replicating protect state per consumer would require generating a distinct frame per subscriber, defeating event distribution and multiplying bandwidth. *(Caveat: this single-counter-per-fan-out property follows from `mw::com`'s pub-sub topology itself, not from where the counter lives — Option B preserves it equally, since the binding still owns exactly one counter per `SkeletonEvent`, not one per consumer. It is not, by itself, a reason to prefer Option A over Option B.)*

*Cons:* Requires `SkeletonEventBinding::Send()` to grow a new parameter and an agreed serialized-header representation between layers. For any byte-buffer-oriented transport (e.g. SOME/IP), the CRC-covered range spans transport-owned prefix bytes — Request ID, Protocol Version, Interface Version, Message Type, Return Code — that the binding-independent layer never constructs and has no reason to know about; supplying them across the boundary would mean either duplicating transport-header assembly outside the binding, or handing the binding-independent layer a scratch buffer it doesn't otherwise need. Also breaks LoLa's zero-copy model: the header would need to be written by one layer into storage conceptually owned by another.

**Option B — Protect state in the binding layer (`SkeletonEventBinding`) — recommended**

The binding owns `ProtectState` and calls `ProtectMessage()` internally during `Send()`, directly on the buffer/slot it already owns.

```text
SkeletonEventBase
    │  calls Send(sample)
    ▼
SkeletonEventBinding (owns E2EProtectContext)      <- counter lives here
    │  calls ProtectMessage() internally, on its own already-assembled buffer/slot
    ▼
Transport
```

*Pros:* No change to `SkeletonEventBinding::Send()`'s signature. The binding already owns every byte the CRC needs to cover — its own transport-specific prefix (if any), the reserved header region, and the payload — so `ProtectMessage()` runs as a single in-place call with no cross-layer buffer handoff. Counter reset is tied directly to the binding's own service-lifecycle hooks (`OfferService()`/`StopOfferService()`), with no signal needed from another layer.

*Cons:* Counter state is untestable without instantiating a binding (mitigated: bindings are already unit-tested independently). Each binding must correctly implement counter-lifecycle management rather than inheriting it once from shared code — mitigated by keeping the CRC/counter *algorithm* itself (not its orchestration) in a shared, binding-independent library every binding links against, the same separation vsomeip keeps between its profile algorithm and its transport-specific protector code (see below).

**Option B is recommended.** The binding is the only party that has the fully-assembled wire buffer at the point `ProtectMessage()` must run, so keeping the call there avoids re-exporting transport-owned header bytes across the binding boundary just to relocate a function call. vsomeip's protector (see [Where the Header Bytes Actually Live](#where-the-header-bytes-actually-live-transmit-path-api-shape) and [Receiver-Side Check State](#receiver-side-check-state)) demonstrates the same call-site choice in an open-source SOME/IP stack — it writes header fields directly into the buffer it already owns, with no separate transport-agnostic orchestrator involved. The binding owns the counter and calls `ProtectMessage()` directly on the buffer it is already assembling; only the reusable CRC/counter *algorithm* is shared, binding-independent code. For SOME/IP specifically this is not just a convenience: several profiles fold wire-level fields the binding-independent layer has no reason to know about — Message ID, Client ID, Session ID, Protocol/Interface Version — into the CRC computation alongside the payload, so whichever party invokes `ProtectMessage()` must already have the fully-assembled SOME/IP header, not just the application payload. That rules out Option A outright for such profiles, independent of the zero-copy/testability arguments above.

> **Note — periodic/mixed transmission-trigger events.** Some SOME/IP event deployments configure `update_on_change: false` (or mixed mode), meaning the provider retransmits on a fixed timer independent of the application calling `Send()` again. Since the E2E counter increments per transmitted frame, that idle retransmission must also re-invoke `ProtectMessage()` on the binding's own `E2EProtectContext`. Because that context is binding-owned, this is entirely internal to the SOME/IP binding's retransmission-timer logic — no cross-layer call is needed to keep the counter and the retransmit scheduler in step. LoLa has no equivalent: it only sends on an explicit app `Send()` call, so this does not apply there.

#### Where the Header Bytes Actually Live (transmit-path API shape)

Unlike the receive path, `Send()` is fire-and-forget — it returns `Result<void>` and never hands the application a handle to enrich or read metadata from afterward (`impl/skeleton_event.h`). So there is no transmit-side equivalent of the "extend `SamplePtr<T>`" question: the application never needs to see the E2E header, and neither `SkeletonEvent::Send(const EventType&)` nor `Send(SampleAllocateePtr<EventType>)` needs to change its app-facing signature. The only question is *where the computed header bytes are physically written*, which grounding in the real LoLa implementation answers concretely:

- **Both `Send()` overloads converge on one internal path.** LoLa's `SkeletonEvent<SampleType>::Send(const SampleType&, ...)` (binding-independent) already allocates a slot, copy-assigns into it, and forwards to `Send(SampleAllocateePtr<SampleType>, ...)`, which in turn forwards to the binding's own `SkeletonEventBinding::Send(SampleAllocateePtr<void>, ...)` (`bindings/lola/skeleton_event.h`). As of [eclipse-score/communication#897](https://github.com/eclipse-score/communication/pull/897) (merged), `SkeletonEventBinding` is a single, non-template, fully type-erased interface — it has no compile-time knowledge of `SampleType` at all; the binding-independent `SkeletonEvent<SampleType>` layer performs the typed ↔ type-erased conversion at the call boundary. So `ProtectMessage()` can run in exactly one place inside the binding's own `SkeletonEventBinding` implementation, after the sample is finalized in the allocated slot and before forwarding to the transport — covering both overloads uniformly, with no new parameter needed on any binding-independent-facing interface, and with no `SampleType` dependency inside the binding itself.
- **No spare space exists in LoLa's control block.** `EventSlotStatus` is deliberately packed into a single `std::atomic<std::uint64_t>` (32-bit timestamp + 32-bit refcount) so it can be updated with a single lock-free CAS (`bindings/lola/event_slot_status.h`). There are no spare bits there to carry a header — extending it would break the atomic-word update model the receive/collector logic depends on.
- **The header does *not* need to live inside `SampleType`, and does not require any codegen change.** LoLa already stores per-slot metadata in a parallel array, separate from the data: `EventDataStorage<SampleType>` (`= DynamicArray<SampleType, ...>`, pure application data) and `EventDataControl` (`= DynamicArray<ControlSlotType, ...>`, i.e. `EventSlotStatus`) are two independently allocated shared-memory arrays of equal length, correlated purely by `slot_index` (`bindings/lola/event_data_storage.h`, `bindings/lola/event_data_control.h`). A third parallel array of the same shape — e.g. `EventDataE2EHeaderStorage = DynamicArray<E2EHeaderBytes, ...>` — allocated alongside the other two at `Register<SampleType>()` time, gives every slot its own fixed-size header region indexed by the same `slot_index`. `ProtectMessage()` then writes into `header_storage_->at(slot_index)` exactly the way `EventSlotStatus::SetTimeStamp()` already writes into the control array for that slot. This mirrors existing, load-bearing LoLa infrastructure rather than introducing a new mechanism, and keeps the E2E header entirely binding-internal — invisible to the application and to code generation.
- **`Offset` is SOME/IP-specific and does not apply to LoLa.** For a byte-buffer-oriented binding (SOME/IP), `Offset` is a mandatory, user-configured byte offset within the serialized wire buffer, since the E2E header shares that buffer with the SOME/IP header and the payload. For LoLa, there is nothing to offset: `EventDataE2EHeaderStorage`'s per-slot record holds *only* the E2E header, at a fixed, implementation-internal position, so a LoLa deployment never needs to set `Offset` at all — the parameter must not be exposed or required in a LoLa consumer's or provider's configuration. Worth calling this out explicitly wherever `Offset` is documented (see [Configuration](#configuration)) so it isn't misread as a parameter every binding needs.
- **This is a LoLa-specific (zero-copy) problem, not a general limitation of this design.** vsomeip's `profile07::protector::protect(e2e_buffer& _buffer, ...)` (`vsomeip/implementation/e2e_protection/src/e2e/profile/profile07/protector.cpp`, purely as an architectural reference) writes header fields into an already-serialized byte buffer at `config_.offset_`, entirely independent of the sender's C++ type. A future SOME/IP binding would do the same: insert the header during marshalling, with no parallel-array or storage change needed. LoLa needs the extra array only because it has no serialization step — the shared-memory slot *is* the application's `SampleType` object.
- **Replication model: follows `EventDataStorage`, not `EventDataControl`.** `EventDataStorage` is single-writer (skeleton), read-only for all consumers regardless of ASIL, and exists once per event. `EventDataControl` is replicated once per ASIL level because `EventSlotStatus` is mutably CAS-updated from both QM and ASIL-B sides, and freedom-from-interference forbids sharing that mutable state across a memory partition. The E2E header is written once by the producer (`ProtectMessage()`) and only ever read afterward — same access pattern as `EventDataStorage` — so `EventDataE2EHeaderStorage` is a single, non-replicated array, sibling to `EventDataStorage`, not folded into `EventDataControlComposite`.
- **Sizing/indexing: per-event, profile-sized, reuses the existing slot-index correlation.** Events are already registered individually via `lola::Skeleton::Register<SampleType>(ElementFqId, SkeletonEventProperties)`, with `SkeletonEventProperties` sizing that event's `EventDataStorage`/`EventDataControl`. Extend `SkeletonEventProperties` with the event's configured profile ID/header width to size `EventDataE2EHeaderStorage` the same way — per-event, not worst-case-across-all-profiles. Indexing reuses the existing `slot_index` correlation: `SampleAllocateePtr::GetReferencedSlot()` (already used by `SkeletonEventCommon::Send()` to index `EventDataControl`) indexes the header array on the transmit side, and the binding's already-known `slot_index` at `SamplePtr` construction indexes it on the receive side. No new members on `SampleAllocateePtr`/`SamplePtr`.

This means the transmit-side responsibility split is simpler than the receive side in three respects: no app-facing API-shape decision, no codegen impact on the application's generated data type (once the parallel-array storage is used), and — since Protect() runs and stays entirely inside the binding under [Option B](#sender-side-protect-state) — no cross-layer header-byte handoff at all. The LoLa-specific piece of work is confined to `EventDataStorage`/`EventDataControl`'s sibling infrastructure, not to `SampleType` itself.

#### Context Ownership Cardinality

> **Scope: events and fields only.** This cardinality asymmetry exists because of pub/sub fan-out, which only events and fields have. Methods are point-to-point — one proxy call, one skeleton response — so cardinality on both sides is trivially one-to-one; see [Protect/Check ownership is bidirectional for methods](#applicability-across-communication-element-types) for how the method case differs instead.

The protect and check/state-machine contexts are not symmetric in cardinality — the transmit side has exactly one context per event stream, while the receive side has one context per consuming instance:

| Context | Cardinality | Owner |
|---|---|---|
| Protect state (`E2EProtectContext`, sender counter) | One per transmitted event (`SkeletonEvent`) | Binding, `SkeletonEventBinding` |
| Check state (`E2ECheckContext`, last-accepted counter, CRC context) | One per consuming `ProxyEvent` instance that enables E2E | Binding, `ProxyEventBinding` |
| Historical health state (`E2EHealthContext`, hysteresis error/recovery counter) | One per consuming `ProxyEvent` instance that enables E2E | Binding-independent, `ProxyEventBase` |

This follows the sender/receiver split: `ProtectMessage()` is sender-local and produces one wire frame per cycle; `CheckMessage()`/the binding-independent health tracker are receiver-local, and each consumer evaluates the same received frame sequence independently. Consequences:

- All consumers of one event observe the *same* counter sequence from the wire; the sender counter must not be replicated or reset per consumer — doing so would require per-subscriber frames, which defeats multicast delivery.
- Each consumer's state machine reacts only to loss/reordering it individually observes: one consumer missing a frame transitions its own state machine while other consumers of the same event are unaffected. This is why check/SM *state* remains per consuming `ProxyEvent` instance regardless of criticality (see below) — different proxies may subscribe/attach at different times and therefore observe different subsets of the sample stream, so a single shared/centralized check context is not correct for LoLa's peer-to-peer shared-memory model.
- Whether Check()/SM *executes* for a consumer and whether that consumer *acts on* the resulting status are separate questions — see "Mixed-criticality precedent" below.

#### Mixed-Criticality Precedent (vsomeip as vendor corroboration)

There is no single mandated mixed-criticality configuration axis; this section instead checks how an existing commercial implementation chose to realize it, as supporting evidence. Cross-referencing an existing SOME/IP E2E implementation (vsomeip, refer at `vsomeip/implementation/e2e_protection/` purely as an architectural reference) shows a simpler pattern than a per-consumer enable/disable flag:

- vsomeip's E2E provider is configured per `(service_id, event_id)`, not per consumer: `CheckMessage()`-equivalent logic runs once per received message whenever the event has E2E configured, and the resulting status (`message::get_check_result()`) is delivered to every local subscriber alongside the payload, regardless of that subscriber's criticality.
- There is no concept of a subscriber statically opting out of the check *executing*; every consumer of an E2E-configured event receives the same computed status, and it is up to the consuming application whether to read and act on it.
- Mixed-criticality handling is therefore a consumption-time decision rather than a separate protection-configuration axis, consistent with the pattern above.

**This simplifies the recommendation:** by default, every consuming `ProxyEvent` that resolves an E2E profile (see [Profile Resolution Is Mandatory, Even When Checking Is Not](#profile-resolution-is-mandatory-even-when-checking-is-not)) runs Check()/health-tracking and receives a populated `E2EResult` (see [Public API Implications](#public-api-implications)) — checking-enabled is the default shape of a resolved profile, not something a consumer must separately opt into. Per the deployment model in [Example Deployment Mapping](#example-deployment-mapping-illustrative), this is expressed as each consuming instance's own, self-contained configuration entry — there is no shared event-level default to inherit from; a consumer that wants the common, checking-enabled behavior simply configures a normal E2E profile in its own entry, duplicating the same values other consumers use. A consumer that does not employ E2E verification — e.g. a QM consumer with no use for the result — is expected to explicitly set `DataIntegrityCheckEnabled: false`, `SequenceCheckEnabled: false`, and `HistoricalHealthTrackingEnabled: false` in that same self-contained entry, and its `E2EResult` fields (and `Summary`) then read `kDisabled`, exactly as if it had no E2E profile at all. This is a first-class, expected per-consumer configuration state — a QM consumer opting out is the common case, not a niche CPU-optimization for outliers. It does not change the check/health-state *cardinality* conclusion above: each consuming `ProxyEvent` instance still owns its own context regardless of whether it enables checking, for the attach-timing reasons already stated.

Example — one provider, three consumers at different ASIL levels, all of the same event:

| Instance | ASIL | Check/health-tracking executes | Consumes result | State owned |
|---|---|---|---|---|
| Provider (skeleton) | — | n/a (always protects) | n/a | One `E2EProtectContext` (counter) |
| Consumer A (proxy) | ASIL_B | yes (default) | yes | Own check/historical-health state |
| Consumer B (proxy) | ASIL_B | yes (default) | yes | Own check/historical-health state |
| Consumer C (proxy) | QM | no — explicitly disabled via `DataIntegrityCheckEnabled`/`SequenceCheckEnabled`/`HistoricalHealthTrackingEnabled: false` | n/a — `E2EResult` reads `kDisabled` | No check/historical-health state instantiated |

If Consumer A misses a frame that Consumer B receives, only Consumer A's historical health tracking reacts (e.g. its error counter increments on a sequence error); Consumer B remains `HistoricalHealthStatus::kOk`. Consumer C has disabled E2E verification entirely in its own configuration, so no Check()/health-tracking runs for it and its `E2EResult` reads `kDisabled` across all fields — the expected, common shape for a QM consumer with no use for the result, not a special-case optimization.

---

### Receiver-Side Check State

The E2E check function `CheckMessage()` requires a **mutable check state** (`CheckState`) that holds the last accepted counter (and other profile-specific reception state) and is updated on every call — the receive-side counterpart to the protect state above. Whichever layer owns this state must be the one invoking `CheckMessage()`; the two responsibilities cannot be split.

#### Options for `mw::com`

**Option A — Check state in the binding-independent layer (`ProxyEventBase`)**

The binding parses the transport-specific message and extracts the protected-data view and E2E header bytes, without invoking any E2E logic, and delivers them to the binding-independent layer. The binding-independent layer owns `CheckState` and calls `CheckMessage()` itself.

```text
Transport
    │
    ▼
Binding                                             <- parses wire format only
    │  extracts protected-data view + E2E header bytes
    ▼
ProxyEventBase (owns E2ECheckContext)               <- check state lives here
    │  calls CheckMessage() → DataIntegrityStatus + SequenceStatus
    ▼
E2E Manager: historical health tracking
```

*Pros:* Single, reusable implementation of profile-specific check logic across all bindings (LoLa, SOME/IP, …), if that logic is not otherwise shared. Symmetric with a binding-independent-owned protect state.

*Cons:* Requires raw byte spans (`protected_data`, `e2e_header`) to cross the binding/binding-independent boundary on every received frame, purely so a call can happen one layer up from where the bytes already live. For SOME/IP event/field profiles those spans must additionally line up with transport-owned prefix bytes that only the binding constructed in the first place while parsing the frame — a boundary that exists only to relocate a function call, not to hide information the binding-independent layer actually needs.

**Option B — Check state in the binding layer — recommended**

The binding owns `CheckState` and calls `CheckMessage()` internally, on the same buffer it already parsed, returning the already-computed `DataIntegrityStatus`/`SequenceStatus` pair, optionally accompanied by the header-stripped payload (`raw_sample`, see [Proposed Contract](#proposed-contract)) — no empty-cycle case is synthesized, see [Binding Independence Assessment](#binding-independence-assessment-review-feedback).

```text
Transport
    │
    ▼
Binding (owns E2ECheckContext)                      <- check state lives here
    │  parses wire format AND calls CheckMessage() internally, on its own buffer
    ▼
[raw_sample (payload, optional)] + DataIntegrityStatus + SequenceStatus
    │
    ▼
E2E Manager (binding-independent): historical health tracking (consumes the two enums only)
```

> **`raw_sample` is not an unconditional output.** `DataIntegrityStatus`/`SequenceStatus` are produced for every checked frame; the payload is only handed up when the frontend has use for it, and what happens to it is decided by the frontend from the payload type declared in the service interface, not by the binding (see [Proposed Contract](#proposed-contract)). It is also empty whenever `DataIntegrityStatus` is `kError`.

*Pros:* No check material (E2E header, protected-data span) ever crosses the binding boundary — the binding already has everything `CheckMessage()` needs, because it just parsed it. Only the two small categorical enums (and, optionally, the header-stripped payload `raw_sample`) cross, and the health tracker consumes just the enums, which are trivially mockable for binding-independent health-tracking tests (see [Testing Strategy](#testing-strategy)).

*Cons:* The profile-check algorithm itself must be reused as a shared library across bindings to avoid duplicating it (addressed below — the algorithm is shared as a library across bindings). The binding is no longer a purely stateless wire-format adapter — it owns `E2ECheckContext` per consumer alongside its own subscription/queue state, a natural extension of state the binding already manages per consumer (e.g. LoLa's per-consumer slot/queue bookkeeping).

**Option B is recommended**, matching the transmit-side decision above.

#### Open-Source Precedent (vsomeip)

vsomeip (an independent open-source SOME/IP stack, cited only as architectural corroboration, `vsomeip/implementation/e2e_protection/`) is built the same way: `profile07::protector::protect(e2e_buffer&, ...)` (`vsomeip/implementation/e2e_protection/src/e2e/profile/profile07/protector.cpp`) writes header fields directly into an already-serialized byte buffer owned by the caller, rather than through a separate transport-agnostic orchestrator, and its E2E provider is configured and invoked per `(service_id, event_id)` inside the routing/notification path itself. What stays genuinely reusable in this design is not the call site, but the underlying profile-check algorithm — the CRC/counter engine — which is linked into that call site as a library rather than duplicated per transport; this is the same functional separation this design relies on: reusable *algorithm*, binding-owned *orchestration and state*.

This precedent is why [Sender-Side Protect State](#sender-side-protect-state) and this section both recommend Option B: keeping Protect/Check orchestration inside the binding — while sharing only the profile algorithm as a library — is a proven separation in at least one production SOME/IP stack, not a hypothetical alternative.

---

### Binding-Independent Responsibilities

The binding-independent layer owns **E2E interpretation and supervision**.

This includes:

- Interpreting the sequence of per-message results the binding produces, over time
- Tracking a hysteresis-based error/recovery counter across received messages
- Combining the current per-message result with historical health into a single application-facing summary
- Producing the final user-visible `E2EResult`

#### Historical Health Tracking

> **Note — relationship to a companion error-handling specification.** The `HistoricalHealthStatus`/`Summary`/`E2EResult` model below mirrors a separate, in-progress companion specification containing the detailed, authoritative specification of E2E error handling. It is reproduced here for this document's own architectural completeness, not as a competing definition; the two are expected to be merged into one once both settle.

`mw::com` events are consumed reactively: an application calls `GetNewSamples()` when it has reason to believe new data is available, not on a fixed periodic schedule independent of activity. If a provider stops offering the service, the proxy resubscribes and the binding-independent layer resets its own tracking state on that signal (see [E2E Context Lifecycle](#e2e-context-lifecycle)) — there is no need to keep feeding a "no new data" result into a health tracker on a cycle where nothing was received, and therefore no need for the dedicated warm-up/no-data states a classical periodic state machine uses to represent them. The binding-independent E2E Supervisor instead tracks a single hysteresis-based error counter, updated only when a message is actually received and checked:

```cpp
enum class HistoricalHealthStatus {
    kDisabled, // Check was disabled
    kOk,       // Error counter is at or below the configured recovery threshold
    kError,    // Error counter has reached the configured error threshold; stays kError until
               // the counter drops back to/below the recovery threshold (hysteresis)
};
```

The counter increments on a failed per-message check (`DataIntegrityStatus::kError` or a `SequenceStatus` error value) and decrements on a passed one, clamped so it never drops below zero. `HistoricalHealthStatus` transitions to `kError` once the counter reaches the configured error threshold, and back to `kOk` only once the counter drops to/below the (lower) recovery threshold — the gap between the two thresholds is the hysteresis band, preventing the status from flapping on a single isolated failure or recovery. See [Configuration](#configuration) for the threshold parameters.

An application typically does not need `HistoricalHealthStatus` on its own — it is combined with the current message's own check outcome into a single summary value:

```cpp
enum class Summary {
    kDisabled,             // All checks were disabled (no E2E verification)
    kOk,                   // All E2E checks passed (full E2E verification)
    kOkWithDisabledChecks, // Enabled checks passed but some were disabled (partial E2E verification)
    kError,                // At least one E2E check failed
};

struct E2EResult {
    DataIntegrityStatus data_integrity;
    SequenceStatus sequence;
    HistoricalHealthStatus historical_health;
    Summary summary;
};
```

`Summary` is a derived, computed value — not independently settable — so it can never drift out of sync with the three underlying fields: `kDisabled` when all three are `kDisabled`; `kError` when any of them reports an error; `kOk`/`kOkWithDisabledChecks` otherwise, depending on whether every enabled check ran or some were individually disabled. `data_integrity`/`sequence`/`historical_health` remain directly accessible for consumers (e.g. forensic/diagnostic logging, see [Public API Implications](#public-api-implications)) that need the finer-grained detail `Summary` intentionally discards.

This is the status exposed to applications.

#### E2E Context Lifecycle

Context ownership splits along the same line as [Context Ownership Cardinality](#context-ownership-cardinality): `E2EProtectContext` (sender) and `E2ECheckContext` (receiver) live in the binding, while `E2EHealthContext` (receiver) lives in the binding-independent layer. Each is reset by whichever layer already has direct visibility into the lifecycle event that invalidates it — no context is reset by a layer that has to be told about the event secondhand.

**Protect/Check context — reset directly by the binding, on its own lifecycle hooks.** The binding already observes its own service-offer and subscription lifecycle natively, so no cross-layer signal is required:

- **Transmit.** The binding resets `E2EProtectContext` on its own `OfferService()`/`StopOfferService()` (or equivalent) — a direct consequence of the binding owning both the protect state and the service-offer lifecycle hooks that bound it, so destroying and recreating the protect state alongside the offer/re-offer cycle requires no signal from any other layer.
- **Receive.** LoLa's binding already implements the equivalent detection today: partial-restart/reconnection is handled by `lola::SubscriptionStateMachine` (`StopOfferEvent()`/`ReOfferEvent()`), driven by `ServiceDiscovery` notifications forwarded through `lola::Proxy`'s `FindServiceGuard` — entirely inside the binding. The binding resets its own `E2ECheckContext` on the same transition it already tracks for subscription-state purposes.

**Historical health context — reset by the binding-independent layer, on a subscription-state-change signal from the binding.** The health counter has no wire-format dependency, so it stays binding-independent — but it still needs to know when communication continuity was intentionally broken, since a reset here is what prevents valid post-restart frames from being misread as a sequence error and driving `historical_health` toward `kError` even though the channel was correctly re-established (a safety-relevant failure mode, not merely cosmetic). This is already surfaced today via `ProxyEventBindingBase::GetSubscriptionState()` and `SetSubscriptionStateChangeHandler()`, which fire synchronously as the subscription transitions `SUBSCRIBED → SUBSCRIPTION_PENDING` (on `StopOfferEvent()`) and back to `SUBSCRIBED` (on `ReOfferEvent()`) (see `score/mw/com/design/skeleton_proxy/README.md` §"Proxy auto-reconnect functionality" and `score/mw/com/design/events_fields/README.md` §"Event subscription"). The binding-independent E2E Supervisor resets `E2EHealthContext` on this existing callback without any new binding interface. Future bindings (SOME/IP, DDS) must provide the equivalent subscription-state signal purely for this purpose — reset of their own `E2EProtectContext`/`E2ECheckContext` is their own internal concern.

These contexts (all three) shall be reinitialized whenever communication continuity is intentionally broken by the receiver, including:

- provider (skeleton) restart,
- service re-offer,
- service instance change,
- subscription re-establishment,
- any event that creates a new logical communication session.

#### Binding Independence Assessment (Review Feedback)

This was checked against the classical windowed state-machine algorithm that a fixed-cycle periodic supervision model would use, which this design deliberately replaces with a simpler hysteresis-based counter (see [Historical Health Tracking](#historical-health-tracking) for the event-driven consumption rationale). Findings:

1. **The historical health counter has no invocation-cadence dependency.** A classical windowed state machine counts window sizes/thresholds in "message cycles" and assumes it is invoked exactly once per configured reception cycle — including cycles where nothing new arrived, which a periodic, polling-style consumer model would require feeding in explicitly. The hysteresis-based counter used here instead only ever updates on a cycle where `GetNewSamples()` actually delivered and checked a real message — there is nothing for it to update on an empty cycle, so no periodic/idle-cycle feeding is required, and no separate timer or scheduler component is needed to drive it. This is a narrower invocation contract than a periodic-polling model would require, adopted deliberately for `mw::com`'s reactive, event-driven consumption model (`GetNewSamples()` called when the application has reason to believe new data is available, not on a fixed schedule).
2. **This does not lose stall/dead-sender detection, because the classical windowed SM never provided it either.** Real SOME/IP deployment data confirms per-event cyclic E2E usage is common in practice (e.g. ASIL-relevant P04/P07-protected events sent on fixed timers at rates from 10ms to several seconds) — but even the classical windowed state machine does not degrade `kValid` on repeated "no new data" cycles alone, so a stalled/dead sender was never caught by windowed counting to begin with. Stall/dead-sender detection is, and remains, the job of subscription/service-availability lifecycle monitoring (`SetSubscriptionStateChangeHandler()`, see [E2E Context Lifecycle](#e2e-context-lifecycle)), not the health tracker.
3. **Recommendation.** The historical health counter runs synchronously inside the binding-independent layer's wrapping of the binding's own `GetNewSamples()`/`Send()` call (the same wrapping point already proposed for E2E result attachment in [Interface Contract Between Layers](#interface-contract-between-layers)) — updated only when a real message was checked. No separate timer or scheduler sub-component needs to be built.

---

## Configuration

The E2E Supervisor's historical health tracking requires per-event configuration supplied from the deployment model: profile-specific parameters for the per-message check, plus a small binding-independent set of hysteresis parameters for the health tracker.

### Historical Health Configuration

| Parameter | Description |
|---|---|
| `ErrorIncrement` | Amount added to the error counter on a failed per-message check |
| `OkDecrement` | Amount subtracted from the error counter (floored at zero) on a passed per-message check |
| `ErrorThreshold` | Error counter value at or above which `HistoricalHealthStatus` becomes `kError` |
| `RecoveryThreshold` | Error counter value at or below which `HistoricalHealthStatus` returns to `kOk` (must be ≤ `ErrorThreshold`; the gap between the two is the hysteresis band) |

### Profile Configuration (profile-specific)

| Parameter | Description |
|---|---|
| Profile ID | Which E2E profile applies |
| Data ID | 16-bit identifier used in CRC computation |
| Data ID mode | How Data ID is included (all, nibble, list — profile-dependent) |
| MaxDeltaCounter | Maximum allowed counter jump before a `kWrongSequence` result |
| Offset | Byte offset of the E2E header within the serialized message — **SOME/IP-specific**; not applicable, and not required in configuration, when the binding is LoLa (shared-memory) — see [Where the Header Bytes Actually Live](#where-the-header-bytes-actually-live-transmit-path-api-shape) |

These parameters must be sourced from the service deployment configuration (a future extension to the existing `impl/configuration/` model) and injected into the binding and the E2E Manager at construction time.

### Profile Resolution Is Mandatory, Even When Checking Is Not

Every consumer's resolved configuration must include the Profile Configuration parameters (Profile ID, Data ID, Offset, …), regardless of whether that consumer performs any E2E check at all. This is not an E2E-checking concern; it is a **deserialization** concern: for a wire-serializing binding (SOME/IP), the E2E header is embedded inline in the received buffer, so the binding must know its profile/offset/width just to locate where the application payload begins, before any check logic runs or is skipped. A consumer that disables checking (see [Per-Consumer Check Enablement](#per-consumer-check-enablement)) still needs its profile resolved for this reason — disabling *checking* only stops `CheckMessage()`/health-tracking from running; it never removes the need to correctly skip past the header bytes on the wire. "Mandatory" here means the resolved value must exist for every consumer — per the deployment model in [Example Deployment Mapping](#example-deployment-mapping-illustrative), each consumer's own deployment entry currently does repeat it verbatim, since there is no shared event-level default to inherit from instead; de-duplicating that repetition is a separate, later optimization, not a correctness requirement.

This does not apply to LoLa: its E2E header lives in a separate, binding-internal parallel array (`EventDataE2EHeaderStorage`, see [Where the Header Bytes Actually Live](#where-the-header-bytes-actually-live-transmit-path-api-shape)), never inline with the application payload, so there is no header-skipping step for LoLa's deserialization to depend on. This requirement is therefore SOME/IP-specific (and applies to any future binding that serializes the header into the same buffer as the payload).

### Per-Consumer Configuration Overrides

A consumer may need parameters that differ from other consumers of the same event — e.g. a consumer that intentionally receives a decimated sample stream needs a wider `MaxDeltaCounter` than other consumers. `mw::com`'s deployment model addresses this by attaching E2E configuration entirely to `ServiceInstance`, described next.

`mw::com`'s E2E configuration is attached entirely to `ServiceInstance` (see [Example Deployment Mapping](#example-deployment-mapping-illustrative) for the resulting schema): every consuming instance declares its own complete, resolved E2E configuration (Profile Configuration, health-tracker tuning, check-enablement flags), rather than inheriting a shared default from the provider/service-interface level and overriding only the delta. This has four implications for the deployment model:

1. Per-consumer tuning applies to Profile/Check parameters (e.g. `MaxDeltaCounter`), not only to health-tracker tuning — the illustrative example in [Example Deployment Mapping](#example-deployment-mapping-illustrative) models this for both `max_delta_counter` and `error_threshold`.
2. Each instance's full configuration is resolved once, at deployment time, from that instance's own static deployment entry (e.g. `mw_com_config.json`) — a plain read of static data at process startup, with no runtime reconfiguration mechanism.
3. This duplicates configuration across instances that happen to share the same effective settings (e.g. every consumer using the default profile repeats that profile in its own entry) — a known, accepted tradeoff at this stage rather than an oversight; optimizing it (e.g. via inheritance or a shared default) is tracked as a separate, later ticket.
4. The same per-instance mechanism also carries the check-enablement flags described below — a consumer's decision to disable some or all of its own E2E checking is part of that same self-contained, per-instance entry.

### Per-Consumer Check Enablement

Three independent flags control whether E2E checking executes for a given consumer, one per `E2EResult` field they gate:

| Flag | Effect | Saves `CheckMessage()` CPU cost? |
|---|---|---|
| `DataIntegrityCheckEnabled` (bool, default `true`) | When `false`, `data_integrity` reads `DataIntegrityStatus::kDisabled`; `sequence`/`historical_health` are unaffected (subject to their own flags below) | No — `CheckMessage()` computes CRC and counter validity together in one call (see [Binding Output](#binding-output)); this flag only discards the CRC half of the already-computed result before it reaches the consumer |
| `SequenceCheckEnabled` (bool, default `true`) | When `false`, `sequence` reads `SequenceStatus::kDisabled`; `data_integrity`/`historical_health` are unaffected (subject to their own flags) | No — same reason as above |
| `HistoricalHealthTrackingEnabled` (bool, default `true`) | When `false`, `historical_health` reads `HistoricalHealthStatus::kDisabled` and no hysteresis counter is maintained for this consumer | Only when both check flags above are also `false` — see below |

These flags are kept separate rather than folded into one combined knob: `CheckMessage()` always computes CRC and counter validity together in one call regardless of these flags (see [Binding Output](#binding-output)), so enabling them independently costs nothing extra at the check layer, and the resulting `Summary::kOkWithDisabledChecks` case (see [Historical Health Tracking](#historical-health-tracking)) is exactly what a consumer enabling only one of the two per-message checks produces.

CPU savings only materialize when both `DataIntegrityCheckEnabled` and `SequenceCheckEnabled` are `false`: with no per-message check result ever produced, the binding never needs to invoke `CheckMessage()` for that consumer's context at all. Setting all three flags to `false` is equivalent to full E2E opt-out for that consumer (`Summary::kDisabled` across the board).

**Invalid combination — rejected by configuration parsing, not a runtime state.** `HistoricalHealthTrackingEnabled: true` combined with `DataIntegrityCheckEnabled: false` and `SequenceCheckEnabled: false` is a misconfiguration: with neither underlying per-message check enabled, the health tracker never receives a check result to update on (see [Historical Health Tracking](#historical-health-tracking)), so its counter would sit permanently at its initial value and report a static, misleading `HistoricalHealthStatus::kOk` for a channel that was never actually verified. `Summary::kError` is not a fit either: `Summary` is purely derived from the three `E2EResult` fields (see [Historical Health Tracking](#historical-health-tracking)), and none of them would organically read `kError` in this state — `data_integrity`/`sequence` read `kDisabled` and `historical_health` reports `kOk` since its counter never moves, so forcing `kError` here would require a special-cased override that breaks that derivation invariant; `kError` is also semantically a transient, per-message signal, whereas this is a permanent, static deployment defect.

This must instead be caught where invalid deployment configuration is already rejected today, in two layers: (1) schema/config-generation-time validation, before the configuration is ever packaged into a deployable artifact; and (2) `impl/configuration/`'s own JSON-parsing step, which already fails fast on structurally invalid per-consumer configuration — e.g. `LolaMethodInstanceDeployment::CreateFromJson()` terminates via a contract violation when a mandatory field is missing (`CreateFromJsonWithoutEnabledFlagTerminates`, `impl/configuration/lola_method_instance_deployment_test.cpp`). Extending that same parsing step to reject this flag combination means even a config that slips past (1) is caught the moment mw::com parses it at process startup — before the affected consumer ever processes a message — not silently carried into operation.

**Note on `DataIntegrityCheckEnabled: false` combined with `SequenceCheckEnabled: true`.** `SequenceStatus` is derived from a counter field inside the same wire buffer whose CRC integrity is no longer being verified in this combination — a consumer choosing it is trusting the counter's value without verifying the buffer it came from wasn't corrupted or tampered with. This is a deliberate, accepted per-consumer tradeoff, not a defect; documented here so it isn't chosen unknowingly.

### Distinguishing Intentional Under-Sampling from Genuine Loss

A consumer that intentionally decimates its consumption (e.g. a 100ms consumer cycle against a 10ms producer emission rate) observes the same wire-level counter delta (~10) as a consumer that genuinely lost 9 frames. Widening `MaxDeltaCounter` per consumer (as above) resolves the false-positive, but proportionally reduces that same consumer's sensitivity to genuine loss over the same margin — an inherent precision/recall limitation of the profile-check algorithm itself, not something this layering can fix by itself.

Whether this is even a real problem for a given event depends entirely on the transport's delivery model — not on any config parameter:

- **Queued/bounded-history transport (LoLa today; the target model for the upcoming SOME/IP binding, see below):** `GetNewSamples()` iterates every unread slot/queue entry in a single call — a slow-polling consumer still has Check()/health-tracking run on every intermediate sample; it only discards results at its own callback logic. As long as the queue/slot depth is provisioned to at least the consumer's intended decimation ratio, the health tracker sees a gapless counter sequence and **no configuration parameter is needed at all** beyond sizing that depth — this is a capacity-planning decision, not a detection mechanism.
- **Last-value/non-queued transport:** a slow poller only ever observes the latest value, so a genuine wire-level counter gap exists on every read. **No config parameter can recover the lost distinction here.** Decimation and real loss are, by construction, the same observable event on this kind of transport — the sample that would have told them apart was already overwritten before anyone read it, so there is no side channel left for any normalization function to consult. A `MaxDeltaCounter` override is therefore not a "sharper" option next to a blanket one — it is the *only* option, and a blanket tolerance that equally forgives real loss of the same magnitude, not as a mechanism that "detects" or "distinguishes" decimation from loss.

Two proposals were considered for the last-value case and rejected:

1. **A dedicated `ExpectedSampleInterval` tolerance parameter**, normalizing `observed_delta` before it reaches the hysteresis-based health accounting. Rejected: since it can never exceed `MaxDeltaCounter`, any delta it forgives already produced `kOk` from the raw profile check regardless of the parameter's existence — it adds a second name and a redundant validation path for exactly the same blanket forgiveness `MaxDeltaCounter` already provides, without adding any actual discriminating power (per the reasoning above: last-value transports leave no information for any parameter to key off of).
2. **Decoupling Check()/health-tracking invocation from the app's polling cadence**, via a background per-consumer mechanism running at wire rate so genuine loss and intentional decimation stay distinguishable regardless of transport. Rejected: for a queued transport this adds nothing, since `GetNewSamples()` already runs Check()/health-tracking once per queued sample and sizing the queue depth already gives full distinguishability. For a last-value transport it does not help either, unless the background poller reliably out-races the producer — at which point it has silently become an inefficient, jitter-fragile reimplementation of a queue, not a new capability. It would also require a new binding-independent background execution context with its own thread/scheduler and synchronization against concurrent `GetNewSamples()` calls — directly reversing the "no separate timer/scheduler needed" conclusion in [Design Rationale](#design-rationale) / [Binding Independence Assessment](#binding-independence-assessment-review-feedback) point 3, for no measurable benefit. Recorded here so it is not re-proposed without this context.

The recommended guidance, replacing any dedicated tolerance parameter:

- **Queued transports:** size queue/slot depth to the consumer's max intended decimation ratio. No E2E-specific configuration needed.
- **Last-value transports:** widen `MaxDeltaCounter` per consumer to the same ratio, with an explicit, documented acceptance of reduced loss-sensitivity over that margin. This is a known, bounded tradeoff — not a gap to be engineered away.

#### Assumption for the Upcoming SOME/IP Binding

No SOME/IP binding exists in `mw::com` yet — `impl/bindings/` currently only contains `lola/` and `mock_binding/` — so its receive-buffering model is an open design decision, not an established fact. It is, however, not a free choice either: `mw::com`'s own `BehaviourOfGetNewSamples` requirement (`score/mw/com/dependability/requirements/component_requirements/component_requirements_ipc.trlc`) already commits every binding to FIFO-drain semantics — repeatedly fetching the next received sample from the underlying receive buffer until `maxNumberOfSamples`/`maxSampleCount` is reached or no new samples remain, and clearing that buffer on `Unsubscribe()` — mirroring the buffering behavior LoLa already implements today. A SOME/IP binding must satisfy the same requirement, just with a different underlying mechanism.

The strongest available open-source evidence that this is achievable over SOME/IP specifically (not just required on paper) comes from vsomeip's own dispatch path (an independent open-source SOME/IP stack, cited only as architectural corroboration):

- `vsomeip`'s per-message notification dispatch (`application_impl::on_message` → the `handlers_` queue, drained in order by dispatcher threads, `implementation/routing/`) already delivers every received notification in arrival order — it does not overwrite or fuse entries, and is a suitable feed for a bounded per-event queue, analogous to LoLa's `numberOfSampleSlots`.
- `vsomeip`'s separate event-value cache (`event::current_`/`update_` in `implementation/routing/src/event.cpp`) is deliberately last-value-only by design (used for late-join/initial-value delivery) — the wrong primitive to build sample history on, confirming the queue must be fed from the notification-dispatch path instead.

**This document assumes the upcoming SOME/IP binding will provide a per-event bounded receive queue** (fed from the notification dispatch path, not the value cache), analogous to LoLa's `numberOfSampleSlots`, making it a queued transport per the guidance above and removing the need for any delta-tolerance parameter. This is not merely the likelier of two equally-plausible designs: `mw::com`'s own `BehaviourOfGetNewSamples` requirement makes FIFO-buffered delivery mandatory for every binding, so a last-value-only SOME/IP binding would be a deviation from that requirement, not a valid alternative implementation of it. The last-value guidance in [Distinguishing Intentional Under-Sampling from Genuine Loss](#distinguishing-intentional-under-sampling-from-genuine-loss) is retained for intentionally-minimal bindings that don't target full conformance with that requirement — it does not apply to a conformant SOME/IP binding by default, and this assumption should only be revisited if such a non-conformant binding is deliberately chosen.

#### Interaction With a Future Payload Serializer (Forward-Looking)

A separate, still-evolving design (`score/mw/com/design/some_ip/serializer_design.md`) proposes a codegen'd, dynamically-loaded per-event codec (`ISerializer`) that encodes/decodes the payload for a future SOME/IP binding. That design is out of scope here and unresolved on its own terms (e.g. its `void*` payload parameter's path to becoming type-aware is still under discussion), but whatever shape it settles into must satisfy the following, since they follow directly from decisions already made in this document:

- **Slot capacity must reserve room for the E2E header in addition to the codec's own payload size.** A codec that reports only its own payload bound (e.g. via a `GetMaxSerializedSize()`-style query) is not sizing for E2E: per [Where the Header Bytes Actually Live](#where-the-header-bytes-actually-live-transmit-path-api-shape), the E2E header for a byte-buffer-oriented binding sits inline in the serialized message at a profile-specific `Offset`, so an E2E-protected event's slot capacity is `transport header size + e2e header size (when the event carries an E2E profile) + codec's reported payload size` — the E2E header size is statically known per Profile ID and must be added by the binding, not folded into the codec's own size query.
- **The codec must not run on rejected bytes.** `CheckMessage()` operates on the raw serialized span and must complete before any decode step is attempted (see [Binding Output](#binding-output) and [Receiver-Side Check State](#receiver-side-check-state)); a message reporting `DataIntegrityStatus::kError` should not be handed to the codec at all, both to avoid decoding bytes E2E has already flagged as corrupted/untrustworthy and because a CRC computed after decoding — over a deserialized, compiler-laid-out object — would not reproduce the wire bytes the sender actually protected (padding is not part of the serialized form and is not portable across peers).
- **The E2E result must attach to whatever handle exists at reception time, before typed decoding.** If the eventual serializer design delivers an untyped/raw sample handle prior to decoding (so that decoding can be deferred or skipped), `GetE2EResult()` (see [API Shape Options](#api-shape-options)) belongs on that raw handle, not only on an already-typed one, since `CheckMessage()` has no dependency on the codec having run.

These are requirements this document's design places on that future integration, not decisions about the serializer design itself; they should be reconciled once that design settles its own open questions.

### Example Deployment Mapping (Illustrative)

The following sketch illustrates how the parameters above could be expressed as an extension of the **actual, existing** `mw_com_config.json` schema. The baseline structure (`serviceTypes`, `serviceInstances`, `instanceSpecifier`, `instanceId`, `asil-level`, `numberOfSampleSlots`, …) is taken directly from [`score/mw/com/impl/configuration/example/mw_com_config_someip.json`](https://github.com/eclipse-score/communication/blob/main/score/mw/com/impl/configuration/example/mw_com_config_someip.json); only the `"e2e"` key is new. The concrete schema for that key is still illustrative and a separate, later design/implementation task.

> **Configuration model.** The E2E deployment model is defined as follows:
>
> 1. **E2E configuration is owned entirely by `ServiceInstance`, not `ServiceType`.** Each `serviceInstances[].instances[].events[]` entry — including the provider and every consumer — carries a complete, self-contained `"e2e"` object. There is no shared default at the `serviceTypes[].bindings[].events[]` level for a consumer to inherit or override. This allows each instance to use a different profile or to disable E2E independently of other instances of the same event. The resulting duplication of identical configuration is accepted for the current design; deduplication through inheritance or shared defaults is deferred to a separate follow-up item.
> 2. **Binding-specific validity is enforced at construction time, not by a shared, all-fields-optional schema.** Parameters that apply only to one binding, such as `offset_bytes` for SOME/IP, are validated during deployment-time config parsing by binding-specific C++ configuration structs. A value that is meaningless for another binding, such as `offset_bytes` in a LoLa configuration, fails during parsing and terminates startup before the affected instance begins running, consistent with the fail-fast validation already used in `impl/configuration/` (see [Per-Consumer Check Enablement](#per-consumer-check-enablement)'s invalid-combination handling).
>
> Within this constraint, the distinction between binding-independent and binding-specific E2E configuration (Historical Health Configuration + check-enablement flags vs. Profile Configuration) remains valid, but it is enforced by the binding-specific C++ struct that parses the JSON, rather than by a nested `binding_independent`/`binding_specific` key inside `"e2e"`.

```json
{
  "serviceTypes": [
    {
      "serviceTypeName": "/vehicle/services/VehicleStateService",
      "version": { "major": 1, "minor": 0 },
      "bindings": [
        {
          "binding": "SOMEIP",
          "serviceId": 4096,
          "events": [
            { "eventName": "VehicleSpeed", "eventId": 32769 }
          ]
        }
      ]
    }
  ],
  "serviceInstances": [
    {
      "instanceSpecifier": "abc/abc/VehicleStateServiceProvider",
      "serviceTypeName": "/vehicle/services/VehicleStateService",
      "version": { "major": 1, "minor": 0 },
      "instances": [
        {
          "instanceId": 1, "asil-level": "B", "binding": "SOMEIP",
          "events": [
            {
              "eventName": "VehicleSpeed", "numberOfSampleSlots": 20, "maxSubscribers": 4,
              "e2e": { "profile": "P04", "offset_bytes": 8, "data_id": 4097, "max_delta_counter": 2 }
            }
          ]
        }
      ]
    },
    {
      "instanceSpecifier": "abc/abc/BrakeControllerProxy",
      "serviceTypeName": "/vehicle/services/VehicleStateService",
      "version": { "major": 1, "minor": 0 },
      "instances": [
        {
          "instanceId": 2, "asil-level": "B", "binding": "SOMEIP",
          "events": [
            {
              "eventName": "VehicleSpeed",
              "e2e": { "profile": "P04", "offset_bytes": 8, "data_id": 4097, "max_delta_counter": 5, "error_threshold": 5 }
            }
          ]
        }
      ]
    },
    {
      "instanceSpecifier": "abc/abc/DiagnosticLoggerProxy",
      "serviceTypeName": "/vehicle/services/VehicleStateService",
      "version": { "major": 1, "minor": 0 },
      "instances": [
        {
          "instanceId": 3, "asil-level": "QM", "binding": "SOMEIP",
          "events": [
            {
              "eventName": "VehicleSpeed",
              "e2e": {
                "profile": "P04", "offset_bytes": 8, "data_id": 4097, "max_delta_counter": 2,
                "data_integrity_check_enabled": false,
                "sequence_check_enabled": false,
                "historical_health_tracking_enabled": false
              }
            }
          ]
        }
      ]
    }
  ],
  "global": { "asil-level": "QM", "applicationID": 100 }
}
```

Every instance's `"e2e"` object carries the full resolved Profile Configuration (`profile`/`offset_bytes`/`data_id`/`max_delta_counter`) its binding needs to run Protect()/Check() and, for SOME/IP, to skip header bytes during deserialization (see [Profile Resolution Is Mandatory, Even When Checking Is Not](#profile-resolution-is-mandatory-even-when-checking-is-not)). The provider and each consumer therefore repeat the same profile, illustrating the accepted duplication tradeoff noted above. `BrakeControllerProxy` (ASIL_B) overrides `max_delta_counter` and supplies its own health-tracker tuning (`error_threshold`), see [Per-Consumer Configuration Overrides](#per-consumer-configuration-overrides). `DiagnosticLoggerProxy` (QM) keeps the full profile (mandatory for SOME/IP deserialization) but disables all three check-enablement flags, so it receives `Summary::kDisabled`.

---

## Interface Contract Between Layers

### Design Goal

The interface shall allow the binding-independent layer to evaluate E2E status **without exposing transport-specific details**.

### Proposed Contract

The binding owns Protect/Check state and invokes `ProtectMessage()`/`CheckMessage()` directly on the buffer/slot it already owns (see [Sender-Side Protect State](#sender-side-protect-state) and [Receiver-Side Check State](#receiver-side-check-state)). What always crosses the binding/binding-independent boundary is therefore a **plain categorical result pair**, never the check material (the E2E header or the protected-data span) — the binding-independent E2E Supervisor combines that pair with its own historical health tracking to produce the final, user-visible `E2EResult`. The header-stripped payload (`raw_sample`) may accompany the pair, but whether and how it is consumed is the frontend's decision, not the binding's (see below).

```cpp
// Receive path: binding has already run CheckMessage() on its own buffer before this crosses the boundary
struct BindingReceivedFrame {
    std::span<std::byte> raw_sample;      // non-owning view over the wire/slot bytes CheckMessage() examined; empty when the binding's own check reported kError; whether it is consumed is the frontend's decision
    DataIntegrityStatus  data_integrity;  // already computed by the binding
    SequenceStatus       sequence;        // already computed by the binding
};
```

> **No longer templated on `T`.** [eclipse-score/communication#1079](https://github.com/eclipse-score/communication/pull/1079) (proxy-side type erasure — pending merge at the time of writing; `main` already carries the skeleton-side equivalent via [#897](https://github.com/eclipse-score/communication/pull/897), see [Where the Header Bytes Actually Live](#where-the-header-bytes-actually-live-transmit-path-api-shape)) makes `ProxyEventBinding` a single, non-template, type-erased interface. `BindingReceivedFrame` reflects that here: `raw_sample` is a non-owning `std::span<std::byte>` — the binding hands up exactly the bytes it just ran `CheckMessage()` on, with no dependency on `SampleType`. The actual zero-copy/reference-counted sample handle is constructed separately, by `ProxyEventBinding::MakeSamplePtr()` (also generalized to `SamplePtr<void>` by #1079), and is later rebound to the app-facing `SamplePtr<const SampleType>` by `ProxyEvent<SampleType>` — `BindingReceivedFrame` itself is only a short-lived, boundary-crossing carrier for the raw bytes plus the already-computed check results, not the sample handle itself.

**`raw_sample` is not an unconditional output, and its use is the frontend's decision.** `data_integrity`/`sequence` are produced for every checked frame; the payload is only worth handing up when the frontend has use for it, and what it does with it follows from the payload type declared in the service interface, not from anything the binding decides:

- **Raw byte payload type ("no deserialization" marker):** the frontend passes the header-stripped payload through to the application as raw bytes.
- **Typed payload with a codec:** the frontend runs `ISerializer::Deserialize()` on the payload.
- **LoLa:** the sample is already a typed object in the shared-memory slot, delivered through `MakeSamplePtr()`, so the frontend need not consume the byte view at all.

In every case `raw_sample` is empty when `data_integrity` is `kError`, so rejected bytes are never decoded or delivered.

**Named `raw_sample`, not `sample`, deliberately.** This is always a byte view over the pre-decode material the binding produced, whether or not any further decoding ever happens: for LoLa it is a byte view over the already-finalized typed object sitting in its shared-memory slot, since there is no wire encoding to undo and no extra copy is made; for SOME/IP it may be a view over still-undecoded bytes, since whether and when to turn it into a typed object is the binding-independent frontend's decision (or the app's, via `SampleView`) — not something the binding presumes on the frontend's behalf.

**The application's declared payload type is not always a deserialized type.** Per the separate payload-serializer design's "Keep `GetNewSamples()` API; choose typed vs raw payload in the service interface" option, a SOME/IP-bound event may opt entirely out of serialization when the application's own C++ data type already matches the wire representation: the event's declared payload type is a raw byte type (e.g. `std::array<std::uint8_t, N>`), paired with a "no deserialization" configuration marker for that event. In that mode, `ISerializer::Deserialize()` is never invoked, and `raw_sample` is a `std::span<std::byte>` over the raw frame with the SOME/IP header and E2E header already stripped off by the binding during parsing — payload bytes only, exactly as `CheckMessage()` saw them. `data_integrity`/`sequence` are populated identically in both modes, since `CheckMessage()` runs on the wire bytes regardless of whether a decode step follows — opting out of serialization only skips the codec, never the E2E check.

There is no transmit-path equivalent struct: the binding computes and writes its own E2E header in place (see [Where the Header Bytes Actually Live](#where-the-header-bytes-actually-live-transmit-path-api-shape)), so `SkeletonEventBinding::Send(sample)` needs no additional parameter at all.

**Transmit path (Application → Wire, top down):**

```text
Application
    │  calls Send(sample)
    ▼
SkeletonEventBinding                                     <- owns E2EProtectContext
    │  calls ProtectMessage() in place, on its own buffer/slot
    │  writes Length/Counter/DataID/CRC directly into the reserved header region
    ▼
Wire / Transport
```

**Receive path (Wire → Application, reverse — read bottom to top):**

```text
Application
    ▲  receives SamplePtr<const SampleType> carrying the result (GetE2EResult())
    │
ProxyEvent<SampleType> (binding-independent)              <- rebinds SamplePtr<void> to SamplePtr<const SampleType>
    ▲
E2E Supervisor (binding-independent)                      <- owns E2EHealthContext on ProxyEventBase
    ▲  combines DataIntegrityStatus + SequenceStatus with tracked health → E2EResult
    │
ProxyEventBinding::GetNewSamples()                        <- owns E2ECheckContext; parses wire format AND calls CheckMessage()
    ▲  produces BindingReceivedFrame{ raw_sample (std::span<std::byte>; use is frontend's decision), data_integrity, sequence }
    │
Wire / Transport
```

**Delivery mechanism — result embedded in `SamplePtr<T>`.**

`SamplePtr<T>` itself carries the final result via a getter (`GetE2EResult() const -> E2EResult`), leaving `ProxyEvent<SampleType>::GetNewSamples()`'s existing callback signature — `void F(SamplePtr<const SampleType>) noexcept` (`impl/proxy_event.h`) — completely unchanged. Because the type-erased `SamplePtr<void>` is constructed by the binding (`ProxyEventBinding::MakeSamplePtr()`, `impl/proxy_event_binding.h`) *before* the binding-independent layer has combined the accompanying `DataIntegrityStatus`/`SequenceStatus` pair with its own historical health tracking, the result cannot be baked in at construction time — it is attached afterward via a binding-independent-only mutator (a friend-restricted `SetE2EResult()`), and only later rebound from `SamplePtr<void>` to the app-facing `SamplePtr<const SampleType>` by `ProxyEvent<SampleType>`.

The attachment point for this already exists as a pattern in the codebase: `ProxyEvent<SampleType>::GetNewSamples()` already wraps the binding's per-sample callback once, for tracing (`tracing::CreateTracingGetNewSamplesCallback()`, `impl/proxy_event.h`), before forwarding to the application's `receiver`. The E2E Supervisor adds one more such wrapping stage — intercept each `SamplePtr<T>` as it comes out of the binding, run its historical health tracking on the accompanying `DataIntegrityStatus`/`SequenceStatus` pair, call `SetE2EResult()` on that same `SamplePtr`, then forward it unchanged to the application's original callback. This reuses an established interception point rather than introducing a new one.

See [API Shape Options](#api-shape-options) for the full trade-off discussion behind this delivery shape.

This contract has several advantages:

- A narrow boundary: a trivially-mockable value pair (`DataIntegrityStatus`/`SequenceStatus`), optionally accompanied by the header-stripped payload — no check material (E2E header, protected-data span) ever crosses into binding-independent code, and the health tracker only ever consumes the pair
- No cross-layer buffer handoff on the transmit path — the binding writes its own header in place
- Deterministic unit testing of historical health tracking: inject `DataIntegrityStatus`/`SequenceStatus` values directly, no transport or binding needed
- Stable separation between transport/profile-check logic (binding + shared profile-algorithm library) and communication-supervision logic (binding-independent health tracker)

---

## Public API Implications

Applications should **not consume the binding's raw `DataIntegrityStatus`/`SequenceStatus` pair directly**.

The binding-independent layer combines the raw per-message results with historical health tracking into a single communication-level result (`E2EResult`).

### API Shape Options

`SamplePtr<T>` is an existing, widely-used `final` class (`impl/plumbing/sample_ptr.h`, `impl/bindings/lola/sample_ptr.h`). Three shapes were considered for attaching E2E status to a delivered sample:

| Approach | Pros | Cons |
|---|---|---|
| `E2ESamplePtr<T>` wrapping/replacing `SamplePtr<T>` | Status travels with the sample; hard to forget to check | Changes `SamplePtr` semantics for all consumers; forces E2E awareness onto QM consumers; complicates generic, transport-agnostic APIs; `GetNewSamples()`'s callback signature (`void(SamplePtr<const SampleType>) noexcept`, `impl/proxy_event.h`) must change to `void(E2ESamplePtr<const SampleType>) noexcept`, breaking every existing application call site whether or not it uses E2E |
| Separate status alongside an unchanged `SamplePtr<T>` | `SamplePtr<T>` stays untouched; works naturally for mixed ASIL/QM deployments on the same event (see [Context Ownership Cardinality](#context-ownership-cardinality)) | Caller must remember to check the status field; two values instead of one |
| `SamplePtr<T>` extended in-place with an additional E2E accessor (status embedded, not wrapped/paired) **(recommended)** | Keeps `GetNewSamples()`'s existing callback signature (`void(SamplePtr<const SampleType>) noexcept`) completely unchanged — no application, including ones ignoring E2E, needs to touch its callback signature at all; cardinality matches naturally (one status per delivered sample per consumer) | Requires a mutator on an otherwise immutable, move-only handle type; status must be attached post-construction via a wrapping callback (mirrors the existing tracing-callback wrapper in `impl/proxy_event.h`, see [Interface Contract Between Layers](#interface-contract-between-layers)); touches every `SamplePtr` construction site (`lola`, `mock_binding`, `mocking/test_type_utilities.h`); does not obviously generalize to method responses if those end up not using `SamplePtr` |

The third option's shape:

```cpp
template <typename SampleType>
class SamplePtr final
{
  public:
    // ... existing members unchanged ...
    E2EResult GetE2EResult() const { return e2e_result_; }

  private:
    friend class ProxyEventBase;  // only the binding-independent E2E Manager may set this
    void SetE2EResult(E2EResult result) { e2e_result_ = result; }
    E2EResult e2e_result_ = E2EResult{DataIntegrityStatus::kDisabled, SequenceStatus::kDisabled,
                                       HistoricalHealthStatus::kDisabled, Summary::kDisabled};
};
```

**The in-place `SamplePtr<T>` extension is recommended, for both LoLa today and a future SOME/IP binding.** The deciding factor is application-facing signature stability: this shape keeps `GetNewSamples()`'s existing callback signature (`void(SamplePtr<const SampleType>) noexcept`) completely unchanged, so no existing application — whether or not it uses E2E — needs to touch its callback to keep compiling. A `SampleWithStatus<T>`/2-arg-callback shape, by contrast, forces every existing application's callback to be rewritten regardless of E2E usage, and introduces a second parallel delivery type that every binding (LoLa, future SOME/IP) and every mock/test utility (`mock_binding`, `mocking/test_type_utilities.h`) would need to support alongside `SamplePtr<T>`. The attachment mechanism itself is already precedented in the codebase: it reuses the same post-construction wrapping-callback pattern already used for tracing (`tracing::CreateTracingGetNewSamplesCallback()`, `impl/proxy_event.h`). Both shapes are otherwise compatible with the mixed-criticality precedent below — `Summary::kDisabled` is expected whenever the *event itself* carries no E2E profile, verification is off, or a consumer has explicitly disabled it via `DataIntegrityCheckEnabled`/`SequenceCheckEnabled`/`HistoricalHealthTrackingEnabled: false` (see [Mixed-Criticality Precedent](#mixed-criticality-precedent-vsomeip-as-vendor-corroboration)) — by default, a consumer of an E2E-configured event that has not disabled checking (e.g. a QM proxy that simply doesn't override anything) still receives a populated, real result and is free not to act on it. This covers events/fields, which use `SamplePtr<T>`; a method Request/Response delivery shape would be a separate design question, since methods may not use `SamplePtr<T>` at all.

```cpp
template <typename T>
struct SampleWithStatus {
    SamplePtr<T> sample;
    E2EResult e2e_result;  // summary == kDisabled when no real check ran for this consumer
};
```

This wrapper shape, or its equivalent as a callback parameter (`GetNewSamples(callback(sample, E2EResult))`), remains a fallback only if a future binding's sample-delivery type cannot support post-construction mutation at all — it is not the recommended default.

### Diagnostic Accessor for Raw E2E Header (Follow-up, Out of Primary Scope)

Review feedback suggested adding an API to retrieve the raw E2E header for debug purposes (bus-level/CRC error triage during development). No existing reference implementation surveyed for this design exposes header content today — only compile-time layout helpers (e.g. header-size queries) — consistent with this design's own categorical-status approach, which only defines a categorical check-status output (see [Historical Health Tracking](#historical-health-tracking)), never raw header content.

Given that, if we pursue this it should be scoped conservatively:

- Diagnostic-only, explicitly non-safety-relevant, with no API-stability guarantee.
- Kept off `SamplePtr<T>` and the primary consumption path — e.g. a `GetLastRawHeader()`-style accessor on the E2E context (last-received snapshot), not a per-sample field.
- Tracked as a follow-up design item, not part of this document's responsibility-allocation decision — it doesn't change the binding/binding-independent split either way. Under the current design, raw header bytes never leave the binding at all (see [Binding Output](#binding-output) and [Interface Contract Between Layers](#interface-contract-between-layers)), so this would need a new, binding-side accessor rather than reusing an existing cross-layer flow.

---

## Testing Strategy

### Binding Layer Tests

Verify:

- Correct placement of E2E fields
- Counter update
- CRC generation
- CRC verification
- Header parsing
- Transport-specific offsets
- `E2EProtectContext`/`E2ECheckContext` reset on the binding's own lifecycle hooks (service (re-)offer, subscription re-establishment — see [E2E Context Lifecycle](#e2e-context-lifecycle))
- `CheckMessage()` invoked only when `GetNewSamples()` actually delivers a new sample — no synthesized empty-cycle result is produced (see [Historical Health Tracking](#historical-health-tracking))
- For a SOME/IP event configured with the "no deserialization" marker, `raw_sample` carries the header-stripped payload bytes directly, `ISerializer::Deserialize()` is never invoked, and `data_integrity`/`sequence` are populated identically to the auto-decoding case (see [Proposed Contract](#proposed-contract))

These tests are independent of historical health tracking, and can be written against the shared profile-algorithm library directly regardless of which binding links it.

### Binding-Independent Tests

Verify:

- Error-counter increment/decrement and hysteresis threshold crossings
- History handling
- Lost-frame detection
- Repeated-frame detection
- Wrong-sequence behavior
- Health context reset on resubscription (see [E2E Context Lifecycle](#e2e-context-lifecycle))

These tests can be executed without any transport or binding implementation at all — `DataIntegrityStatus`/`SequenceStatus` are plain enums, and injecting them directly is now the *only* way any test reaches the health tracker, since the tracker consumes only the enums and never the payload or check material.

Given the [Binding Independence Assessment](#binding-independence-assessment-review-feedback), tests must also cover:

- A stalled sender produces no calls at all, so the health counter is unaffected — stall detection is exercised via the subscription-state-change path instead (see [E2E Context Lifecycle](#e2e-context-lifecycle))
- Hysteresis behavior at the threshold boundary (error counter oscillating around `ErrorThreshold`/`RecoveryThreshold` does not flap `HistoricalHealthStatus`)
- Per-consumer `MaxDeltaCounter` override: applied only to the overriding consumer's own context, leaving the provider's default and every other consumer's context unaffected (see [Per-Consumer Configuration Overrides](#per-consumer-configuration-overrides) and [Distinguishing Intentional Under-Sampling from Genuine Loss](#distinguishing-intentional-under-sampling-from-genuine-loss))
- Per-consumer check-enablement flags (`DataIntegrityCheckEnabled`, `SequenceCheckEnabled`, `HistoricalHealthTrackingEnabled`, see [Per-Consumer Check Enablement](#per-consumer-check-enablement)): each, set to `false` independently, produces `kDisabled` on its own corresponding `E2EResult` field without affecting the others or other consumers of the same event; setting all three to `false` produces `Summary::kDisabled`
- Configuration parsing rejects `HistoricalHealthTrackingEnabled: true` combined with `DataIntegrityCheckEnabled: false` and `SequenceCheckEnabled: false` at startup, before the consumer processes any message (see [Per-Consumer Check Enablement](#per-consumer-check-enablement))

### Integration Tests

Verify:

- End-to-end transmit/receive behavior
- Interaction between binding and historical health tracking
- Correct user-visible result generation
- Cross-binding consistency (LoLa vs. SOME/IP)
- On QNX, subscription-state-change detection latency (LoLa's `ServiceDiscovery` relies on `inotify`, cyclically polled on QNX by `fsevmgr` via the `fse-period=msecs` `io-blk` parameter, unlike Linux) stays bounded well below the minimum provider restart-to-first-frame time, so `E2ECheckContext`/`E2EHealthContext` reset always completes before post-restart frames arrive — do not assume this margin, measure it
- **Deferred:** an equivalent QNX-vs-Linux latency check for the future SOME/IP binding's subscription-state-change/queue-fill notification path (see [E2E Context Lifecycle](#e2e-context-lifecycle) and [Assumption for the Upcoming SOME/IP Binding](#assumption-for-the-upcoming-someip-binding)). The queued/drop-oldest receive model decided there fixes eviction semantics only, not the underlying notification mechanism's latency — that mechanism isn't chosen yet, so this test case cannot be made concrete until the binding's design settles

---

## Safety Classification Considerations

E2E processing may become part of the safety argument for ASIL communication paths.

### Binding Layer

The binding layer manipulates:

- Counter fields
- CRC fields
- Serialized safety-relevant data

If used in an ASIL communication path, the binding implementation responsible for E2E field generation/checking becomes part of the safety-related software.

### Binding-Independent Layer

The historical health tracker determines whether a received sample is considered valid.

If this decision gates delivery of safety-relevant events, the E2E manager/health tracker also becomes part of the safety-related software.

Therefore both components may require the same ASIL classification as the communication path they protect.

This document does not assign a fixed ASIL level; classification depends on the system-level safety concept.

---

## Design Rationale

The chosen allocation follows the `mw::com` layering principles:

| Responsibility                       | Binding Layer | Binding-Independent Layer |
| ------------------------------------ | ------------- | ------------------------- |
| Wire-format / header offset          | ✔             |                           |
| E2E header injection (transmit)      | ✔             |                           |
| E2E header extraction (receive)      | ✔             |                           |
| CRC computation (`ProtectMessage`)   | ✔ (calls shared profile-algorithm library) |            |
| Counter increment / protect state    | ✔ (owns `E2EProtectContext`) |               |
| CRC / counter verification (`CheckMessage`) | ✔ (owns `E2ECheckContext`) |         |
| Raw per-message result (`DataIntegrityStatus`/`SequenceStatus`) | ✔ (produced by `CheckMessage`) | consumed by historical health tracking |
| Payload hand-off (`raw_sample`, header-stripped) | ✔ (strips transport/E2E headers, optionally hands up payload bytes; empty on `DataIntegrityStatus::kError`) | ✔ frontend decides whether to use it, deserialize it, or pass it through raw |
| Health-tracker config (error/recovery thresholds) |               | ✔                         |
| Historical health tracking (hysteresis counter) |          | ✔ (updated only on an actually-checked message) |
| Invocation trigger | ✔ (runs `CheckMessage()`/`ProtectMessage()` inside its own `GetNewSamples()`/`Send()`, only when a real message is present) | ✔ (updates the health counter on the resulting `DataIntegrityStatus`/`SequenceStatus` inside the same wrapping call — no separate timer) |
| Hysteresis-based error/recovery counting |               | ✔                         |
| User-visible result (`E2EResult`) |       | ✔                         |

The binding owns *orchestration and state* for Protect/Check; what remains genuinely reusable across bindings is the *profile algorithm itself* (CRC/counter computation) as a shared, binding-independent library every binding links against — a reusable library shared across bindings, and the same separation vsomeip keeps between its profile-check algorithm and its transport-specific protector code (see [Receiver-Side Check State](#receiver-side-check-state)).

This separation ensures:

- a reusable, once-tested profile-algorithm library underneath every binding's Protect/Check orchestration,
- reusable E2E supervision (historical health tracking and hysteresis accounting) across all bindings,
- minimal cross-layer contract surface — two categorical enums plus, only when the frontend needs it, the header-stripped payload; never the E2E header or protected-data span,
- independent testing of protocol handling and state management,
- and alignment with the existing `mw::com` architecture that separates transport bindings from communication management logic.

> **Note:** the under-sampling-vs-loss guidance ([Configuration](#distinguishing-intentional-under-sampling-from-genuine-loss)) and the diagnostic header accessor ([Public API Implications](#diagnostic-accessor-for-raw-e2e-header-follow-up-out-of-primary-scope)) are additive clarifications/refinements within the layers above — neither changes the responsibility split in this table. Per-event receive-queue depth (the recommended replacement for a tolerance parameter) is a binding-layer capacity/provisioning setting, analogous to LoLa's existing `numberOfSampleSlots`; it does not move any E2E responsibility across the split either.
