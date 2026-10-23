  # E2E Error Handling — Requirements & Public API

  ## Index

  1. [Requirements](#1-requirements)
  2. [Overview](#2-overview)
  3. [Design rationale](#3-design-rationale)
  4. [Deployment-time configuration & states](#4-deployment-time-configuration--states)
  5. [Public API reference](#5-public-api-reference)
  6. [Usage examples](#6-usage-examples)
  7. [Benefits](#7-benefits)
  8. [No-new-data and periodic invocation](#8-no-new-data-and-periodic-invocation)

  ---

 Finalized proposal for the `mw::com` E2E error-handling public API covering the requirements and accepted design.

  ## 1. Requirements
  how applications actually consume E2E-protected samples across multiple
  communication bindings:

  | # | Requirement |
  |---|---|
  | R1 | Expose an E2E result **per sample**. |
  | R2 | Provide a **detailed result** (separate per-check status fields) rather than reducing to a single binary outcome — a full-diagnostic view and a simple accept/reject view must both be derivable from the same result. |
  | R3 | Correctly categorize every possible check outcome into the exposed status enumeration — no outcome is silently dropped or merged into an unrelated category. |
  | R4 | Allow configuring the maximum tolerated sequence gap per deployment. |
  | R5 | Never conflate the **warm-up phase** (no prior sample to compare against yet) with a genuine fault — the first sample must not be misreported as an error. |
  | R6 | Preserve masquerading/corruption detection (metadata/CRC-based) as a status distinct from sequence-based faults (loss/reorder/repetition). |
  | R7 | Stay **binding-agnostic** — the same result model must apply uniformly regardless of the underlying transport/binding, without requiring a separate implementation per binding. |

  Additional constraints derived from consumer analysis:

  - **Contract, not resolution policy:** this API defines *what status a
    consumer receives per sample*, not a fixed algorithm for what to do with
    a detected fault. Consumers resolve faults differently on purpose
    (discard-and-keep-last-good, escalate to diagnostics, persist for
    forensic analysis, aggregate into a broader health signal) — the API
    must support all of these without imposing one strategy.
  - **E2E protection is a deployment-time decision, not a
    runtime-detectable one.** Whether a given consumer performs
    verification is fixed by its configuration; the result accessor must
    always return a concrete value and never require a "not configured"
    sentinel.
  - **Historical-health/state tracking is optional, not mandatory** —
    data-integrity/sequence verification must be able to run independently
    of it.
  - **Configuration is scoped per consumer, not per producer/event** — the
    same protected data stream can be received by multiple consumers with
    different verification settings (e.g. one consumer with full
    verification, another with verification disabled entirely), and each
    consumer's result reflects only its own configuration.

  ---

  ## 2. Overview

  Every received sample carries a single `E2EResult` struct with five fields:
  `data_integrity`, `sequence`, `freshness`, `historical_health`, and an
  aggregated `summary`. `SamplePtr<T>::GetE2EResult()` always returns a concrete value,
  because whether E2E protection is active for a given
  proxy is a **deployment-time** configuration decision, never something that
  needs to be detected at runtime.

  **Where this lives in the codebase:** `GetE2EResult()`/`SetE2EResult()` are
  an extension to the existing `SamplePtr<T>` type (`ProxyEventBase` is the
  only friend allowed to populate the result). No new top-level component is
  introduced — a developer receiving samples through a proxy event already has
  access to this API without any extra wiring.

  The E2E result evaluation flow (header verification, per-check status,
  historical health and the reduced/detailed consumer views) is shown in
  [e2e_result_evaluation.puml](e2e_result_evaluation.puml).

  **Evaluation order:** `data_integrity` is evaluated first, then `sequence`.
  If `data_integrity == kError`, the sequence check is bypassed (the counter
  of a corrupted sample is not trustworthy) and the data-integrity error is
  fed directly into historical-health tracking. `sequence` is then reported
  as the existing `kDisabled` value, so
  `E2EResult` always carries a concrete value. The failed sample is still
  flagged through `data_integrity == kError` and `summary == kError`.
  `freshness` is evaluated after `sequence` and before `historical_health`
  (see §8.3).

  ## 3. Design rationale

  The error classes are kept separate (`data_integrity`, `sequence`,
  `historical_health`) so each underlying concept stays easy to reason about;
  `summary` is provided purely as a simplification for callers that just want
  to know whether a sample is "fine to use". It is derived from all three
  fields (`data_integrity`, `sequence` and `historical_health`): it is
  `kError` if any enabled check failed, regardless of which field it came
  from.

  This separation also matches the wire format itself, not just the API: not
  every profile sends a CRC and a sequence counter together — some profiles
  carry a CRC without a counter — so `data_integrity` and `sequence` can
  correspond to genuinely distinct checks depending on the configured
  `E2EProfile`.

  ## 4. Deployment-time configuration & states
  the wire format carries no flag for it, so header presence/layout is fixed
  entirely by the configured `E2EProfile`. Disabling verification
  does not remove the header from the wire, so de-/serialization must still
  account for it whenever `E2EProfile != None`.

  | Configuration | Scope | Meaning |
  |---|---|---|
  | `E2EProfile` | Sender & receiver | Selects the E2E profile (header layout/length); `None` = no E2E header |
  | `DataIntegrityCheckEnabled` | Receiver only | Enables the check producing `data_integrity`; `false` → `kDisabled` |
  | `SequenceCheckEnabled` | Receiver only | Enables the check producing `sequence`; `false` → `kDisabled` |
  | `HistoricalHealthTrackingEnabled` | Receiver only | Enables the hysteresis check producing `historical_health`; `false` → `kDisabled` |
  | `FreshnessCheckEnabled` | Receiver only | Enables the check producing `freshness`; `false` → `kDisabled`. Default `false` when `E2EProfile == None`, otherwise `true` |
  | `MaxSampleAge` | Receiver only | Maximum age of the last accepted sample (duration); required when `FreshnessCheckEnabled == true`, missing/zero is a misconfiguration |

  `DataIntegrityCheckEnabled` and `SequenceCheckEnabled` are independent
  knobs, not a combined one, since not every profile carries a CRC and a
  counter together. `DataIntegrityCheckEnabled` covers the CRC and the
  Data ID (a profile configuration parameter that is part of the CRC
  computation), so no separate Data ID knob exists; `SequenceCheckEnabled`
  covers the counter only.

  **Defaults & misconfiguration:**

  - `E2EProfile == None` defaults all three to `false`; any other profile
    defaults all three to `true` (each independently overridable).
  - Enabling `DataIntegrityCheckEnabled`/`SequenceCheckEnabled` without a
    profile, or enabling `HistoricalHealthTrackingEnabled` while both other
    checks are off, is a misconfiguration.
  - Misconfigurations are rejected at load time — startup fails fast with a
    clear error, so `GetE2EResult()` never has to handle an invalid
    configuration at runtime.

  **Deployment states:**

  | Case | `data_integrity` | `sequence` | `historical_health` | `summary` | Action |
  |---|---|---|---|---|---|
  | Both checks + historical health enabled | computed | computed | computed | `kOk` / `kError` | Accept only if `summary == kOk` |
  | Both checks + historical health enabled, `data_integrity == kError` (bypass) | `kError` | `kDisabled` (skipped) | computed (error fed from `data_integrity` only) | `kError` | Reject — sequence check skipped |
  | Both checks enabled, historical health disabled | computed | computed | `kDisabled` | `kOkWithDisabledChecks` / `kError` | Decide from `data_integrity`/`sequence` |
  | Only data-integrity + historical health enabled | computed | `kDisabled` | computed (from `data_integrity` only) | `kOkWithDisabledChecks` / `kError` | Decide from `data_integrity`/`historical_health` |
  | Only data-integrity enabled | computed | `kDisabled` | `kDisabled` | `kOkWithDisabledChecks` / `kError` | Decide from `data_integrity` alone |
  | Only sequence + historical health enabled | `kDisabled` | computed | computed (from `sequence` only) | `kOkWithDisabledChecks` / `kError` | Decide from `sequence`/`historical_health` |
  | Only sequence enabled | `kDisabled` | computed | `kDisabled` | `kOkWithDisabledChecks` / `kError` | Decide from `sequence` alone |
| Profile configured, both checks disabled | `kDisabled` | `kDisabled` | `kDisabled` | `kDisabled` | Application-specific — verification skipped, consumer decides |
| Profile not configured, both checks disabled | `kDisabled` | `kDisabled` | `kDisabled` | `kDisabled` | Same as above |
| Misconfiguration: check enabled without a profile | error value if enabled, else `kDisabled` | error value if enabled, else `kDisabled` | `kDisabled` | `kError` | Rejected at load time — check cannot be enabled without a valid profile |
| Misconfiguration: historical health enabled with both checks disabled | `kDisabled` | `kDisabled` | `kError` | `kError` | Rejected at load time — historical-health tracking requires at least one underlying check enabled |

For the two "only one check enabled + historical health enabled" cases, the
hysteresis counter behind `historical_health` is fed only by the pass/fail
outcome of whichever single check is enabled — the other, disabled check
contributes nothing to it.

The sequence bypass applies only when `DataIntegrityCheckEnabled` is `true`
and the check returns `kError`. If data integrity is disabled (e.g. a
sequence-only deployment), the bypass is never taken and `sequence` is
always evaluated. On the bypass path the hysteresis counter receives only the
data-integrity error; no sequence outcome is fed to it. `data_integrity` and
`sequence` are always returned in `E2EResult`, independent of whether
historical health is enabled.

The historical-health misconfiguration deliberately sets `historical_health
= kError` to flag it, even though no underlying check ever fed the
counter — unlike `data_integrity`/`sequence` (which fall back to their
neutral `kDisabled` value when their own check is off), enabling
historical-health tracking with nothing to track is itself unsafe.

The "both checks disabled" case looks identical to the consumer whether or
not a profile is configured, but differs on the wire: with a profile, an
E2E header is still present and must be skipped during deserialization;
without one, there never was a header. `summary == kDisabled` never means
"safe to use" — see Usage below.

  ## 5. Public API reference

  ### 5.1 Enums and types

  ```cpp
  enum class DataIntegrityStatus {
    kDisabled, // Check was disabled
    kOk,       // Metadata (incl. Data ID) and CRC match expectations — data is trustworthy
    kError,    // Metadata (incl. Data ID) or CRC differ from expectations — data is corrupted or forged
};

enum class SequenceStatus {
    kDisabled,                 // Check was disabled (also reported when skipped after data_integrity == kError)
    kOk,                       // In order, no loss (or this is the first sample)
    kOkGapWithinThreshold,     // Some loss, but within the allowed gap
    kErrorRepeated,            // Same counter as the previous sample
    kErrorGapExceedsThreshold, // Gap too large — too much loss/reorder
};

enum class SampleFreshnessStatus {
    kDisabled,              // Check was disabled
    kWaitingForFirstSample, // No sample received yet — normal start-up, not a fault
    kFresh,                 // Last accepted sample is not older than MaxSampleAge
    kStale,                 // Last accepted sample is older than MaxSampleAge — no new data in time
};

enum class HistoricalHealthStatus {
    kDisabled, // Check was disabled
    kOk,       // Error counter at/below recovery threshold — channel healthy
    kError,    // Error counter at/above error threshold — channel unhealthy (hysteresis applies)
};

enum class Summary {
    kDisabled,             // Nothing was checked (E2E fully disabled)
    kOk,                   // All checks were enabled and all of them passed
    kOkWithDisabledChecks, // Enabled checks passed, but some checks were off
    kError,                // At least one enabled check failed
};

struct E2EResult {
      DataIntegrityStatus data_integrity;
      SequenceStatus sequence;
      SampleFreshnessStatus freshness;
      HistoricalHealthStatus historical_health;
      Summary summary;
  };
  ```

  ### 5.2 `SamplePtr<T>` extension

  ```cpp
  template <typename T>
  class SamplePtr final {
    public:
    // Returns the E2E result attached to this sample; always has a concrete value.
    E2EResult GetE2EResult() const { return e2e_result_; }
  private:
    // Only ProxyEventBase (binding-independent E2E manager) may attach the result.
    friend class ProxyEventBase;
    // Called internally once the E2E check has run for this sample.
    void SetE2EResult(E2EResult result) { e2e_result_ = result; }
    // Stores the E2E result for this sample.
    E2EResult e2e_result_;
};
```

## 6. Usage examples

  ### 6.1 Reduced view

  Most simple consumers only ever look at `summary` — a single field is
  enough to decide accept/reject:

  ```cpp
  void SimpleConsumer::OnSampleReceived(SamplePtr sample) {
      const auto e2e_result = sample->GetE2EResult();  // always present

    if (e2e_result.summary == Summary::kDisabled) {
        log_info("E2E fully disabled for this sample");
        process_sample(*sample);
    } else if (e2e_result.summary == Summary::kOkWithDisabledChecks) {
        log_info("E2E partial verification: data_integrity=" + to_string(e2e_result.data_integrity) +
                  ", sequence=" + to_string(e2e_result.sequence));
        process_sample(*sample);
    } else if (e2e_result.summary == Summary::kOk) {
        log_info("E2E result trusted: all checks passed");
        process_sample(*sample);
    } else {
        log_error("E2E status not safe: data_integrity=" + to_string(e2e_result.data_integrity) +
                  ", sequence=" + to_string(e2e_result.sequence) +
                  ", historical_health=" + to_string(e2e_result.historical_health));
        // REJECT
    }
}
```

**Application-specific handling:** `summary == kDisabled` or
`kOkWithDisabledChecks` only means some or all checks didn't run — the API
doesn't decide the sample is safe to use. What a consumer does with it
(accept as-is, fall back to the individual
`data_integrity`/`sequence`/`historical_health` fields, reject) is an
application/safety decision.

### 6.2 Detailed view

Some consumers need the full, undiluted status per sample instead of a
reduced accept/reject decision — e.g. persisting E2E status alongside every
recorded sample for later forensic/offline analysis:

  ```cpp
  void DiagnosticConsumer::OnSampleReceived(SamplePtr sample) {
      const auto e2e_result = sample->GetE2EResult();  // always present

      // Persist every field regardless of summary — nothing is discarded.
      persist_sample_with_metadata(*sample, e2e_result);

      if (e2e_result.summary == Summary::kError) {
          log_warn("Recorded sample failed E2E verification");
      }
  }
  ```

  ## 7. Benefits

  - Single accessor, all info in one struct → simpler caller logic
- `data_integrity`, `sequence`, and `historical_health` always
  accessible, plus an aggregated `summary` for simple consumers → detailed
  diagnostics available without forcing every caller to combine the fields
  themselves
- `GetE2EResult()` always returns a concrete value — E2E is a
  deployment-time decision (see §4)
- Caller can freely ignore whichever field it isn't interested in and
  rely on `summary` alone — no code bloat from having two accessors, since
  full evaluation needs all fields anyway

## 8. No-new-data and periodic invocation

E2E is evaluated when a sample arrives, so if the producer stops sending
(crash, broken link, blocked channel), nothing is checked and nothing is
reported — the consumer keeps using old data without knowing it. This is
neither corruption (`data_integrity`) nor a counter fault (`sequence`), so
it gets its own status: `freshness` (`SampleFreshnessStatus`, see §5.1).

### 8.1 No-new-data: `freshness`

| Value | Meaning |
|---|---|
| `kDisabled` | Freshness check is off |
| `kWaitingForFirstSample` | No sample received yet — normal start-up, not a fault |
| `kFresh` | Last accepted sample is not older than `MaxSampleAge` |
| `kStale` | Last accepted sample is older than `MaxSampleAge` — no new data in time |

Rules:

- An accepted sample sets `kFresh` and resets the age timestamp. A sample
  with `data_integrity == kError` does not reset it.
- `kWaitingForFirstSample` is not fed into `historical_health` (R5).
- A missed deadline is fed into `historical_health` once, not once per query.
- `summary == kError` when `freshness == kStale`.
- `freshness` is independent of the other checks; `sequence` stays
  counter-based only (R6).

### 8.2 Periodic invocation

Assumption: no fixed-period check inside `mw::com`. Freshness is defined by
time (`MaxSampleAge`), not by how often the check is called, and the age is
evaluated when a sample arrives or when the consumer asks. No timer or
thread is needed, and the result is the same for any polling rate.

Two accessors cover the case where no sample arrives:

```cpp
// Evaluates the age at call time; works without a new sample.
E2EResult ProxyEvent::GetE2EChannelStatus() const;

// Optional: called once when the channel turns stale and once when it recovers.
// Backed by one one-shot timer per proxy event, reset on every accepted sample.
Result<void> ProxyEvent::SetStaleHandler(StaleHandler handler);
```

When stale, `data_integrity` and `sequence` keep the values of the last
evaluated sample, `freshness == kStale` and `summary == kError`.

Both accessors are optional ways to consume the same freshness state; the
check itself is controlled only by `FreshnessCheckEnabled` / `MaxSampleAge`:

- `GetE2EChannelStatus()` — pull: the consumer asks when it needs the status
  (polling loop, or before using the last value).
- `SetStaleHandler()` — push: the consumer is notified on `kFresh` → `kStale`
  and `kStale` → `kFresh`, without polling.

Consumers that do not use either still get `freshness` on every received
sample.

### 8.3 Layer and evaluation order

| Layer | Checks |
|---|---|
| Binding-specific | `data_integrity`, `sequence` (checks on the received message) |
| Binding-independent (`ProxyEventBase`) | `freshness`, `historical_health`, `summary` |

`freshness` is time-based and does not look at the payload, so it sits in the
binding-independent layer and works the same for every binding (R7).

Order: `data_integrity` → `sequence` → `freshness` → `historical_health` →
`summary`.
