# E2E Error Handling Requirements for the `mw::com` Public API

## 1. Purpose

This document explains what the `mw::com` public API needs to do to expose
End-to-End (E2E) protection results to consumers, based on the AUTOSAR
standard. It lists what AUTOSAR requires for E2E error reporting, and the
rules the API design must follow.


## 2. Background

E2E (End-to-End) protection is an AUTOSAR safety mechanism. It detects
communication faults between a sender and a receiver:

- **Data corruption** — found using a CRC (checksum).
- **Message loss, repetition, or wrong order** — found using a sequence counter.

The `mw::com` frontend must pass these results on so that safety-relevant
consumers can react to communication faults. The main downstream consumer is
**Nautilus**. Nautilus expects **full AUTOSAR compliance**, since it currently
gets its E2E results from the Vector stack.

## 3. Downstream Consumer Expectations (baseline, already collected)

Derived from the current Nautilus implementation
(`aas/communication/receiver`):

| Aspect              | Current expectation                                              |
|---------------------|-----------------------------------------------------------------|
| Source of truth     | AUTOSAR-compliant E2E results (Vector stack)                    |
| Configuration       | Only the **max delta of the sequence counter** is configurable |
| State machine       | Some errors ignored while the state machine is **not yet ready**|
| Consumer-facing API | **Binary per-sample** result: `E2E_OK` / any error             |

These are the baseline rules that the AUTOSAR review below must confirm and refine.

## 4. AUTOSAR Standard — E2E Error Reporting

### 4.1 Relevant specifications

- **AUTOSAR E2E Protocol Specification** (E2E profiles, CRC, counter).
- **AUTOSAR Specification of E2E State Machine** (`E2E_SMCheck`).
- **AUTOSAR `ara::com` / Adaptive Communication Management** (how E2E results
  are exposed to the application via the proxy/event API).

### 4.2 Per-sample check status (`E2ECheckStatus`)

The standard defines a status for each sample. The API must be able to expose
these values:

| AUTOSAR `E2ECheckStatus` | Meaning                                              |
|--------------------------|------------------------------------------------------|
| `Ok`                     | Sample valid; counter within allowed delta.          |
| `Repeated`               | Sample counter identical to a previously received one.|
| `WrongSequence`          | Counter delta larger than allowed (gap / reorder).   |
| `Error`                  | CRC / data-ID error — sample corrupted.              |
| `NotAvailable`           | No E2E check performed yet (initial state).           |
| `NoNewData`              | No new data available since last check.               |

**Constraint:** AUTOSAR defines these as separate statuses at the check level.
The API must, at a minimum, map each one correctly to a result the consumer
can use.

### 4.3 State machine status (`SMState` / profile-level)

The E2E State Machine looks at per-sample checks over a sliding window of
samples and produces one overall state:

| AUTOSAR `SMState` | Meaning                                                      |
|-------------------|--------------------------------------------------------------|
| `Valid`           | Enough good samples in the window — communication trusted.    |
| `NoData`          | No data received yet.                                         |
| `Init`            | State machine initialized but not enough data to decide yet.  |
| `Invalid`         | Too many faulty samples in the window — communication faulty. |

**Constraint:** The `Init` / `NoData` states mean the "state machine is not
ready yet" phase. During this phase, AUTOSAR expects the receiver to **not
treat the communication as failed**, because there is no reference point yet
to compare against.

### 4.4 Configurable parameters required by the standard

The standard defines configuration parameters that control E2E behavior. At
least the following matter for the API design:

- **`MaxDeltaCounter`** — the biggest allowed gap between two counter
  values in a row. (This is the "max delta of the counter" that Nautilus
  already exposes today.)
- **Window size / thresholds** (`WindowSize`, `MinOkStateInit`,
  `MaxErrorStateInit`, etc.) — control when the state machine changes state.

**Constraint:** `MaxDeltaCounter` is the main setting that must be
configurable through the API. Window/threshold parameters are usually fixed
by the profile, but the API must still respect them.

## 5. Known AUTOSAR API Shortcoming and Required Handling

### 5.1 First-sample false positive

For the **first sample received after subscribing**, the E2E state machine
has no earlier counter value to compare against. Because of this, the AUTOSAR
API reports a `WrongSequence` status — but this is a **false alarm**. It just
means the state machine has not warmed up yet, not that there is a real error.

The AUTOSAR API has **no clean way** to tell this bootstrap case apart from a
real sequence error. Nautilus works around this by **ignoring `WrongSequence`
on the very first sample only**, so the first valid message is not wrongly
rejected.

Reference: `aas/communication/receiver/e2e_validator.cpp` (first-sample
suppression logic).

**Constraint for `mw::com`:** The API design must handle this startup gap.
Either:
- show the state machine's "not ready / init" state clearly, so consumers can
  tell it apart from a real error, **or**
- copy the same suppression behavior, so the first valid sample is never
  reported as an error.

## 6. Requirements Summary for the `mw::com` API

| #  | Requirement                                                                                     |
|----|-------------------------------------------------------------------------------------------------|
| R1 | The API SHALL expose an E2E protection result **per sample**.                                    |
| R2 | The API SHALL, at minimum, provide a **binary result** (`E2E_OK` / any error) to consumers, matching Nautilus expectations. |
| R3 | The API SHALL correctly map every AUTOSAR `E2ECheckStatus` value to the exposed result.          |
| R4 | The API SHALL allow configuration of the **max counter delta** (`MaxDeltaCounter`).              |
| R5 | The API SHALL respect the E2E **state machine** semantics, including the "not yet ready" (`Init`/`NoData`) phase, and SHALL NOT report communication as failed during this phase. |
| R6 | The API SHALL handle the **first-sample `WrongSequence` false positive** — either by exposing the init state or by suppressing the first-sample error. |
| R7 | The API design SHALL remain **AUTOSAR-compliant**, as the primary downstream consumer (Nautilus) expects AUTOSAR-standard behavior. |

## 7. API Sketch and Examples (illustrative)

The examples below show how the requirements would look to an application
developer. They are illustrative and align with
[E2E_RESPONSIBILITY_SPLIT.md](E2E_RESPONSIBILITY_SPLIT.md).

### 7.1 The result types

```cpp
namespace mw::com::e2e {

// Per-sample status (mirrors AUTOSAR E2ECheckStatus)
enum class E2ECheckStatus : std::uint8_t {
  kOk,             // valid
  kRepeated,       // duplicated counter
  kWrongSequence,  // counter delta > MaxDeltaCounter (loss/reorder)
  kError,          // CRC / data-ID error (corruption)
  kNotAvailable,   // no check performed yet (initial state)
  kNoNewData       // no new data since last check
};

// Aggregated state machine state (mirrors AUTOSAR SMState)
enum class SMState : std::uint8_t { kValid, kNoData, kInit, kInvalid };

// Default consumer-facing binary result (R2)
enum class E2EResult : std::uint8_t { kOk, kError };

struct Result {
  SMState        sm_state;      // overall trust
  E2ECheckStatus check_status;  // this sample
};

}  // namespace mw::com::e2e
```

### 7.2 The deterministic binary reduction (R5, R6)

```cpp
// Reproduces the Nautilus special handling:
//  - do not fail while the state machine is not yet ready (Init / NoData)
//  - otherwise collapse every non-Ok status into a single error
mw::com::e2e::E2EResult ToBinary(const mw::com::e2e::Result& r) {
  using mw::com::e2e::SMState;
  using mw::com::e2e::E2ECheckStatus;
  using mw::com::e2e::E2EResult;

  if (r.sm_state == SMState::kInit || r.sm_state == SMState::kNoData) {
    return E2EResult::kOk;  // R5/R6: not-ready phase is not a fault
  }
  return (r.check_status == E2ECheckStatus::kOk)
             ? E2EResult::kOk
             : E2EResult::kError;  // R3: any other status => error
}
```

### 7.3 Consumer view — the common case (binary, R1/R2)

```cpp
auto samples = proxy.my_event.GetNewSamples(...);
for (const auto& sample : samples) {
  if (sample.GetE2EResult() == mw::com::e2e::E2EResult::kOk) {
    Use(sample.Value());          // safe to use
  } else {
    DiscardAndDegrade();          // any error => do not trust this sample
  }
}
```

### 7.4 Consumer view — detailed status (optional)

```cpp
const auto detail = sample.GetE2EDetail();   // full AUTOSAR-style result
if (detail.check_status == mw::com::e2e::E2ECheckStatus::kWrongSequence) {
  LOG_WARN("Lost/reordered samples: counter delta exceeded MaxDeltaCounter");
}
```

### 7.5 Configuration — only the counter delta (R4)

```json
{
  "event": "BrakePressure",
  "e2e": {
    "max_delta_counter": 1   // AUTOSAR MaxDeltaCounter: allow at most +1 gap
  }
}
```

> `max_delta_counter = 1` means: if two consecutive received counters differ by
> more than 1 (e.g. 5 then 7), the sample is `WrongSequence` → binary **error**.

## 8. Open Points / To Confirm Against Official Spec

- Exact set of `E2ECheckStatus` / `SMState` values per the E2E profile in use.
- Whether consumers require **granular** status (beyond binary) for any use case.
- The actual `MaxDeltaCounter` and window/threshold values currently used by
  Nautilus's Vector-stack configuration — these are not mandated by the spec,
  so we need to pull the concrete numbers from the existing config, not the
  standard.

> Note: the question of whether init-phase handling should be explicit in the
> API or hidden behind the binary result is **already resolved** — see the
> Design Recommendation in §9 (Option B: hidden by default, with an optional
> detailed API for advanced consumers).

## 9. Design Decision: How `mw::com` Handles First-Sample / Init-Phase Errors

### 9.1 The core design choice

The AUTOSAR API quirk (reporting `WrongSequence` on the first sample due to lack
of state machine warm-up) means `mw::com` had to pick one of three approaches.
The table below is kept for traceability, showing why the alternatives were
not chosen:

| Option | Approach | Pros | Cons |
|--------|----------|------|------|
| A: Explicit init state | Expose `SMState` to consumers; let them filter `Init` errors themselves | Full transparency; consumers make their own choice | Burden on consumer code; requires education |
| **B: Suppress (like Nautilus) — chosen** | Hide the quirk: suppress first-sample `WrongSequence`, treat as `Ok` | Seamless for consumers; matches Nautilus behavior; minimal false alarms; already proven in production | Hides real state info; less transparent |
| C: Hybrid (offered as an add-on) | Provide binary result by default (suppresses); optional detailed status for advanced consumers | Best of both worlds; backward compatible | Slightly more implementation work |

### 9.2 Decision: **Option B (Suppress), with Option C offered as an optional add-on**

`mw::com` **will** ignore first-sample `WrongSequence` errors in the
binding-independent layer, the same way `e2e_validator.cpp` does today. This is
not an open suggestion — it is the approach already running successfully in
production via Nautilus/Vector stack, so `mw::com` replicates it directly.

**Why:**
1. **Matches Nautilus.** Nautilus already expects this behavior. If `mw::com`
   changes it, Nautilus would see false startup failures.
2. **Simpler for consumers.** The first-sample startup quirk is an AUTOSAR
   implementation detail. Application code should not need to know about it.
3. **Already proven.** Nautilus already uses this approach successfully, so
   there is no need to invent a new solution.
4. **Safety matters.** False alarms during startup are dangerous in
   safety-critical systems. Ignoring known false alarms is the safer choice.

**How to implement it:**
- When `SMState == Init` or `SMState == NoData`, treat every per-sample
  status as non-fatal (not an error).
- Once the first valid (non-error) sample arrives, switch to normal
  validation.
- Optionally log the startup phase for debugging (e.g. a `.GetInitPhaseInfo()`
  method for advanced diagnostics).

**Optional add-on (Option C, hybrid):**
- If some consumers need more detail for debugging, offer an **optional API**
  that returns the full AUTOSAR status and state machine state.
- Example: `sample.GetE2EDetail() → { check_status, sm_state }` (shown in
  section 7.4).
- Keep this **optional and marked as not guaranteed**, so the main API stays
  simple.

### 9.3 Why not Option A?

Showing the init state directly would mean every consumer has to write their
own startup-handling code. Nautilus (our reference consumer) already ignores
this case, so exposing it in `mw::com` would either:
- break Nautilus (if `mw::com` changes its default behavior), **or**
- force Nautilus to add extra filtering on top of `mw::com` (extra, unneeded
  complexity).

Showing the explicit state is useful for *advanced* consumers, but it should
not be the default behavior of the API.

---

## 10. References

- Current Nautilus implementation: `aas/communication/receiver`
- AUTOSAR API shortcoming & special handling:
  `aas/communication/receiver/e2e_validator.cpp` (first-sample suppression)
- AUTOSAR E2E Protocol Specification — Document ID 849,
  `AUTOSAR_FO_PRS_E2EProtocol`, AUTOSAR Foundation, Release R25-11
  (verified against; see `E2ECheckStatus`/`SMState`/`MaxDeltaCounter`
  definitions in §4.2–4.4 and the first-sample `WrongSequence` mapping in §5.1)
- AUTOSAR Specification of E2E State Machine (part of the above document,
  Chapter 6.20)
- AUTOSAR Communication Management (`ara::com`)
