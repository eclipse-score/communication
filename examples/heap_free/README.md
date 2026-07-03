# Heap-Free mw::com Examples

These examples demonstrate how to use `mw::com` skeleton and proxy APIs without heap
allocation. They serve as a reference for components with ASIL-B or FFI heap-allocation
constraints.

For an overview of the `mw::com` module, its architecture, and the LoLa shared-memory
transport, see the [Communication Module README](../../score/mw/com/README.md).

## Two-Phase Execution Model

Each example follows a two-phase model:

1. **Init phase** — heap allocation is permitted. All `mw::com` setup happens here:
   runtime initialization, skeleton/proxy creation, `OfferService()`, `FindService()`,
   `Subscribe()`.
2. **Operational phase** — heap allocation is forbidden. Only heap-free `mw::com`
   operations are used: zero-copy event send/receive via shared memory, field updates,
   and subscription state queries.

The boundary is enforced by `heap_check::forbid_heap()`. See
[Heap-Free Verification](#heap-free-verification) for details.

## mw::com API Heap Behavior

### Init phase only (allocates)

| API | Reason |
|-----|--------|
| `InstanceSpecifier::Create()` | Internal string processing |
| `Skeleton::Create()` / `Proxy::Create()` | Allocates event and field bindings |
| `OfferService()` | Sets up shared-memory bindings |
| `FindService()` | Returns `ServiceHandleContainer` (`std::vector`) |
| `Subscribe()` | Sets up the shared-memory subscription binding |
| `SetReceiveHandler()` | Allocates internally (see [Polling vs. Receive Handler](#polling-vs-receive-handler)) |
| `Field::Update()` before `OfferService()` | Stores initial value on the heap |

### Operational phase (heap-free)

| API | Mechanism |
|-----|-----------|
| `Allocate()` | Returns a slot in the shared-memory ring buffer |
| `Send(SampleAllocateePtr)` | Zero-copy: notifies subscribers of the written slot |
| `Field::Update()` after `OfferService()` | Writes directly to shared memory via the LoLa binding |
| `GetNewSamples()` | Reads from shared memory; callback receives a `SamplePtr` into SHM |
| `GetSubscriptionState()` | Atomic read |
| `Unsubscribe()` | Tears down binding without allocation |

### Subscriber Detection

The `mw::com` skeleton API does not currently provide a method to query subscriber
count or detect subscriptions from the skeleton side. `Send()` and `Allocate()` succeed
regardless of whether any proxy is subscribed, events are written to shared memory
either way.

For systems that require skeleton-proxy synchronization across LoLa domains, the
[LoLa inter-domain Gateway](../../score/mw/com/gateway/README.md) bridges services
and forwards subscription notifications between domains. For SOME/IP-based network
bridging, see the [SOME/IP Gateway](https://github.com/eclipse-score/inc_someip_gateway).

### Polling vs. Receive Handler

`SetReceiveHandler()` allocates internally and cannot be called in the operational
phase. The `event_field_update/proxy` example uses polling with `GetNewSamples()`
instead. If your design requires a receive handler, call `SetReceiveHandler()` during
the init phase before `forbid_heap()`.

## Examples

| Directory | Binary | Demonstrates |
|-----------|--------|--------------|
| `event_send_receive` | `skeleton` | Zero-copy event send loop |
| `event_send_receive` | `proxy` | `FindService`, `Subscribe`, subscription state check |
| `event_field_update` | `skeleton` | Event send + field `Update()` each cycle |
| `event_field_update` | `proxy` | Polling with `GetNewSamples` for event and field |

**Pairing:** start the skeleton before the proxy.

- Pair A: `event_send_receive/skeleton` + `event_send_receive/proxy` (instanceId 1)
- Pair B: `event_field_update/skeleton` + `event_field_update/proxy` (instanceId 11)

## Build

```sh
cd examples/
bazel build //heap_free/...
```

## Run

Remove stale shared-memory segments from previous runs if needed:

```sh
rm -f /dev/shm/lola-*
```

### Pair A

```sh
# Terminal 1
bazel run //heap_free/event_send_receive:skeleton
# Terminal 2
bazel run //heap_free/event_send_receive:proxy
```

### Pair B

```sh
# Terminal 1
bazel run //heap_free/event_field_update:skeleton
# Terminal 2
bazel run //heap_free/event_field_update:proxy
```

Binaries can also be run directly with a config path:

```sh
./bazel-bin/heap_free/event_send_receive/skeleton heap_free/event_send_receive/etc/mw_com_config.json
```

Both binaries exit with code 0 on success. An abort indicates a heap allocation
occurred during the operational phase.

## Heap-Free Verification

`heap_check/heap_check.h` replaces the global `operator new` with a version that
calls `std::abort()` if the calling thread has entered the operational phase via
`forbid_heap()`. The check is per-thread — library background threads (e.g., LoLa
message-passing threads) are unaffected.

Include `heap_check.h` in exactly one translation unit per binary. Call
`allow_heap()` before cleanup to permit heap allocation during teardown.

## Configuration

Each binary has its own `etc/mw_com_config.json`. Skeleton configs declare
`allowedProvider`; proxy configs declare `allowedConsumer`. Both sides of a pair
share the same `serviceId` (7000) and `instanceId`.

## ASIL-B Production Considerations

These examples are configured at ASIL-B level. Teams adapting these patterns for
production should additionally:

- Register MISRA deviations for `heap_check.h` in the project deviation record
- Notify the safety monitor before calling `std::abort()` on heap violation
- Add watchdog or timeout supervision to blocking waits
- Verify that post-`forbid_heap()` cleanup paths (`StopOfferService`, `Unsubscribe`,
  destructors) are heap-free in the target deployment
