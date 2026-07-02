# Heap-Free mw::com Examples

These examples show how to use the S-CORE `score::mw::com` skeleton and proxy APIs
without allocating heap memory in the operational (real-time) phase — relevant for
teams with ASIL-B or FFI heap-allocation constraints.

## Overview

| Directory | Binary | Role | Demonstrates |
|---|---|---|---|
| `event_send_receive` | `skeleton` | Server | Zero-copy event send loop; no heap after init |
| `event_send_receive` | `proxy` | Client | FindService and Subscribe in init; subscription-state check in operational phase |
| `event_field_update` | `skeleton` | Server | Event + field update each cycle; field `Update()` is heap-free after `OfferService` |
| `event_field_update` | `proxy` | Client | Polling with `GetNewSamples` for event and field; no heap after init |

**Pairing for a working run:**

- Pair A: `event_send_receive/skeleton` + `event_send_receive/proxy` (instanceId 1)
- Pair B: `event_field_update/skeleton` + `event_field_update/proxy` (instanceId 11)

Always start the skeleton binary before the matching proxy binary.

## Build

```sh
cd examples/
bazel build //...
```

## Run

If a previous run crashed, remove stale shared-memory segments first:

```sh
rm -f /dev/shm/lola-*
```

### Pair A

```sh
# Terminal 1
bazel run //event_send_receive:skeleton
# Terminal 2 (after the skeleton is running)
bazel run //event_send_receive:proxy
```

### Pair B

```sh
# Terminal 1
bazel run //event_field_update:skeleton
# Terminal 2 (after the skeleton is running)
bazel run //event_field_update:proxy
```

You can also run binaries directly, passing the config path manually:

```sh
./bazel-bin/event_send_receive/skeleton event_send_receive/etc/mw_com_config.json
```

Both binaries should exit with code 0. An unexpected abort indicates a heap
allocation occurred during the operational phase.

## How Heap-Free Verification Works

`heap_check/heap_check.h` replaces the global `operator new` with a version that
calls `std::abort()` if `heap_check::forbid_heap()` has been called on the calling
thread. Each example marks the init/operational boundary with `forbid_heap()` and
re-enables heap use before teardown with `allow_heap()`. The check is per-thread,
so library background threads (e.g. LoLa's message-passing threads) are unaffected.
Include `heap_check.h` in exactly one translation unit per binary (`skeleton.cpp` /
`proxy.cpp`).
