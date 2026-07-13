# Heap-Free mw::com Examples

These examples demonstrate how to use `mw::com` skeleton and proxy APIs without heap
allocation. They serve as a reference for components with ASIL-B or FFI heap-allocation
constraints.

For an overview of the `mw::com` module, its architecture, and the LoLa shared-memory
transport, see the [Communication Module README](../../score/mw/com/README.md).

## Two-Phase Execution Model

Each example follows a two-phase model:

1. **Init phase**: heap allocation is permitted. All `mw::com` setup happens here:
   runtime initialization, skeleton/proxy creation, `OfferService()`, `FindService()`,
   `Subscribe()`.
2. **Operational phase**: heap allocation is forbidden. Only heap-free `mw::com`
   operations are used: zero-copy event send/receive via shared memory, field updates,
   and subscription state queries.

The boundary is enforced by `heap_check::forbid_heap()`. See
[Heap-Free Verification](#heap-free-verification) for details.

## mw::com API Heap Behavior

### Init Phase (allocates)

| API | Reason |
|-----|--------|
| `InstanceSpecifier::Create()` | Internal string processing |
| `Skeleton::Create()` / `Proxy::Create()` | Allocates event and field bindings |
| `OfferService()` | Sets up shared-memory bindings |
| `FindService()` | Returns `ServiceHandleContainer` (`std::vector`) |
| `StartFindService()` | Allocates internally (`make_shared` for handler storage); must be called before `forbid_heap()` |
| `StopFindService()` | Tears down the discovery registration |
| `Subscribe()` | Sets up the shared-memory subscription binding |
| `SetReceiveHandler()` | Allocates internally (see [Polling vs. Receive Handler](#polling-vs-receive-handler)) |
| `Field::Update()` before `OfferService()` | Stores initial value on the heap |

### Operational Phase (heap-free)

| API | Mechanism |
|-----|-----------|
| `Allocate()` | Returns a slot in the shared-memory ring buffer |
| `Send(SampleAllocateePtr)` | Zero-copy: notifies subscribers of the written slot |
| `Field::Update()` after `OfferService()` | Writes directly to shared memory via the LoLa binding |
| `GetNewSamples()` | Reads from shared memory; callback receives a `SamplePtr` into SHM |
| `GetSubscriptionState()` | Atomic read |

### Cleanup Phase

After the operational loop, examples call `allow_heap()` to re-enable heap allocation before teardown. `Unsubscribe()`, `StopOfferService()`, and skeleton/proxy destructors run in the cleanup phase (heap permitted). They are not demonstrated as heap-free operational APIs.

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
| `start_find_service` | `proxy` | `StartFindService` async discovery, wait-before-`forbid_heap` |
| `bidirectional_discovery` | `application_a` | Both applications start concurrently: each offers its own service instance and discovers the other via `StartFindService` without deadlocking |
| `bidirectional_discovery` | `application_b` | Symmetric counterpart of `application_a` |

**Ordering:** Pairs A and B use blocking `FindService` polling; start the skeleton first. Pairs C and D are order-flexible.

* Pair A: `event_send_receive:skeleton` + `event_send_receive:proxy` (instanceId 1; skeleton first)
* Pair B: `event_field_update:skeleton` + `event_field_update:proxy` (instanceId 11; skeleton first)
* Pair C: `event_send_receive:skeleton` + `start_find_service:proxy` (instanceId 1; order-flexible; skeleton lives in the `event_send_receive` group)
* Pair D: `bidirectional_discovery:application_a` + `bidirectional_discovery:application_b` (instanceIds 20, 21; order-flexible)

## Bi-directional Discovery / StartFindService

`FindService()` polling blocks until the service appears. When two apps each wait for
the other's service before calling `OfferService()`, neither proceeds; both time out.

`StartFindService()` is non-blocking: it registers a callback and returns immediately,
so each app can call `OfferService()` before discovery completes.

```
StartFindService(handler, specifier)   // registers callback, returns immediately
OfferService()                         // non-blocking; other side can do the same
wait for discovery callback            // Notification.waitForWithAbort(timeout, token)
// callback has called Proxy::Create() — proxy is ready
forbid_heap()
```

**Heap-free discipline:** `StartFindService()` and `Proxy::Create()` (called inside the
callback) both allocate. The main thread must wait for the callback to complete
**before** calling `forbid_heap()`. If `forbid_heap()` races with an in-progress
`Create()`, the process aborts.

Dynamic reconnection after `forbid_heap()` is not supported. `Proxy::Create()` always
allocates.

`examples/heap_free/start_find_service/proxy.cpp` demonstrates the full pattern paired
with `event_send_receive/skeleton`. `examples/heap_free/bidirectional_discovery/` takes
this further: both `application_a` and `application_b` are simultaneously a skeleton and
a proxy for each other, proving the pattern is deadlock-free even under mutual dependency.

## Build

```sh
cd examples/
bazel build //heap_free/...
```

## Run

Start the provider application and the consumer application. Both binaries exit 0 on success. An abort indicates a heap allocation in the operational phase.

### event_send_receive

What must be running: `:skeleton` (provider) and `:proxy` (consumer). The skeleton exits 0 on its own. The proxy polls `FindService` and fails after 5 seconds if no skeleton is found. Start skeleton first.

```sh
bazel run //heap_free/event_send_receive:skeleton
bazel run //heap_free/event_send_receive:proxy
```

### event_field_update

What must be running: `:skeleton` (provider) and `:proxy` (consumer). Same ordering requirement as `event_send_receive`: the proxy polls `FindService` and requires the skeleton to be reachable. Start skeleton first.

```sh
bazel run //heap_free/event_field_update:skeleton
bazel run //heap_free/event_field_update:proxy
```

### start_find_service

What must be running: `event_send_receive:skeleton` and `start_find_service:proxy`. There is no skeleton in the `start_find_service` directory. The proxy discovers the service offered by `event_send_receive/skeleton` (instanceSpecifier `/sensor/event_send_receive/SensorInterface`, instanceId 1).

This pair is order-flexible. The proxy uses `StartFindService` and waits up to 5 seconds for the skeleton to appear, so either binary may start first.

```sh
bazel run //heap_free/event_send_receive:skeleton
bazel run //heap_free/start_find_service:proxy
```

### bidirectional_discovery

What must be running: `:application_a` and `:application_b`. Neither exits 0 on its own. Each application calls `OfferService` before `StartFindService`, so each is discoverable the moment the other starts discovery. Start in any order.

```sh
bazel run //heap_free/bidirectional_discovery:application_a
bazel run //heap_free/bidirectional_discovery:application_b
```

## Heap-Free Verification

`heap_check/heap_check.h` replaces the global `operator new` with a version that
calls `std::abort()` if the calling thread has entered the operational phase via
`forbid_heap()`. The check is per-thread. Library background threads (e.g., LoLa
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

* Register MISRA deviations for `heap_check.h` in the project deviation record
* Notify the safety monitor before calling `std::abort()` on heap violation
* Add watchdog or timeout supervision to blocking waits
* Verify that post-`forbid_heap()` cleanup paths (`StopOfferService`, `Unsubscribe`,
  destructors) are heap-free in the target deployment
