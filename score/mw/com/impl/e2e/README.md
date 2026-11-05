# POC: E2E historical health with NoNewData supervision

This POC lets a consumer detect a silent sender (no new samples) through the E2E historical health without counting
missing samples as E2E failures. It builds on PR #1445 (per-event `HealthTracker` in `ProxyEventBase`).

## Design decisions

- The binding owns Protect/Check and the check state. It only reports `RawCheckOutcome{data_integrity, sequence}`.
- Historical health is binding independent: one `HealthTracker` per consuming `ProxyEvent`, inside `ProxyEventBase`.
- NoNewData follows AUTOSAR (PRS_E2E_00345, SMAddStatus): a period without new data is a **neutral slot**. It never
  counts as a failure, so `error_threshold` is only driven by real CRC / sequence failures.
- A stalled sender is detected by an optional "too few passed samples" rule (`min_ok_threshold`, AUTOSAR
  `MinOkStateValid`), not by the error count.
- No timer or thread inside mw::com. The application calls `EvaluateE2EHealth(now)` in its own cycle and passes the
  time.

## Wiring

```text
 Application (owns the cycle and the clock)
     |  GetNewSamples(handler, n)                     EvaluateE2EHealth(now)
     v                                                    |
 ProxyEvent / GenericProxyEvent                            |
     |  e2e_receiver -> AttachE2EResult(sample)            |
     v                                                    v
 ProxyEventBase (binding independent, one per consumer event)
     +-- HealthTracker            window of passed / failed / neutral slots -> kOk | kError
     +-- ReceptionTimeoutMonitor  counts elapsed reception_timeout periods, no clock inside
     ^
     |  SamplePtr + RawCheckOutcome{data_integrity, sequence}
     |  GetHealthTrackerConfiguration()  (read once at construction)
 ProxyEventBinding (LoLa, SOME/IP, mock)
     +-- Protect / Check and check state
```

## Health window and states

| Input | Slot |
|---|---|
| Sample with all executed checks OK | passed |
| Sample with CRC/Data ID or sequence error | failed |
| `reception_timeout` period without new data | neutral (neither passed nor failed) |

```text
            failed >= error_threshold
            or (window full and passed < min_ok_threshold)
  +-----+  ---------------------------------------------->  +--------+
  | kOk |                                                   | kError |
  +-----+  <----------------------------------------------  +--------+
            passed >= recovery_threshold
```

`EvaluateE2EHealth(now)`:

```text
  tracker disabled or not subscribed            -> return current status
  sample delivered since last call
    or samples pending (not yet fetched)        -> monitor.OnActivity(now), return current status
  otherwise                                     -> tracker.RecordNoNewData(monitor.ConsumeMissedPeriods(now))
```

Supervision starts with the first new data after `Subscribe()`. A user (re)subscribe resets tracker and monitor.

## Configuration (`HealthTrackerConfiguration`)

| Field | AUTOSAR equivalent | Rule (checked at construction when enabled) |
|---|---|---|
| `window_size` (W) | `WindowSizeValid` / `WindowSizeInvalid` | `1..64`, `>=` both thresholds |
| `error_threshold` (E) | `MaxErrorStateValid + 1` | `E + R > W` (no oscillation) |
| `recovery_threshold` (R) | `MinOkStateInvalid` | see above |
| `min_ok_threshold` (M) | `MinOkStateValid` | `M <= W - E + 1`, so failures alone still switch exactly at E. 0 = off |
| `reception_timeout` | sender cycle / deadline | `>= 0`, 0 = off. Requires `M > 0` |

From a healthy window, `kError` is reached after `W - M + 1` periods without data. Fault detection time:

```text
  t_FD ~= (W - M + 1) * reception_timeout + application evaluation period
```

Example: W=12, E=5, R=10, M=6 -> kError after 7 missed periods. 4 failures plus a few missed periods do not trip the
error threshold; only too few passed samples in the full window do.

## Usage

```cpp
// Application cycle, on the thread that calls GetNewSamples().
proxy.my_event.GetNewSamples(handler, kMaxSamples);
if (proxy.my_event.EvaluateE2EHealth(std::chrono::steady_clock::now()) == score::mw::com::HistoricalHealthStatus::kError)
{
    // Degrade: sender silent or too many E2E failures.
}
```

## Known deviations and open points

- AUTOSAR also treats REPEATED / WRONGSEQUENCE as neutral; this tracker counts them as failures (stricter, as in
  #1445). To be documented as a deliberate deviation.
- `min_ok_threshold` and `reception_timeout` are not parsed from the deployment configuration yet; LoLa still returns a
  disabled configuration. Tests set it through the mock binding.
- `ProxyField` does not expose `EvaluateE2EHealth` yet.
- `mw_com_e2e_binding.md` (Binding Independence Assessment) still states that the classical state machine does not
  degrade on NoNewData; to be corrected.

## Tests

```sh
bazel test //score/mw/com/impl/e2e/... //score/mw/com/impl:proxy_event_e2e_attach_test //score/mw/com:api_surface_test
```
