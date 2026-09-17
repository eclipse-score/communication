# Findings: missing count/length/memory specifications in `message_passing`

**Status:** engineering findings / technical debt log, not yet formalized as requirements.
This document intentionally does **not** introduce TRLC requirements. It records gaps discovered while
analyzing `ServiceProtocolConfig`, `IClientFactory::ClientConfig`, and `IServerFactory::ServerConfig` for
missing specification of argument counts, lengths, and the resulting memory footprint, so that the
findings are not lost and can be turned into requirements/tests deliberately later.

Primary interfaces under review:
[service_protocol_config.h](../../service_protocol_config.h),
[i_client_factory.h](../../i_client_factory.h),
[i_server_factory.h](../../i_server_factory.h).

## 1. `ServiceProtocolConfig::identifier` has no documented length limit, and the two backends disagree

`identifier` is a `std::string_view` with no documented bound in
[service_protocol_config.h](../../service_protocol_config.h). Both backends impose a hard, undocumented,
and *different* limit, with different failure behavior:

- **Unix Domain Sockets** (always used with an *abstract* socket address, `isAbstract = true`, see
  [unix_domain_engine.cpp](../../unix_domain/unix_domain_engine.cpp) and
  [unix_domain_server.cpp](../../unix_domain/unix_domain_server.cpp)):
  [`UnixDomainSocketAddress`](../../unix_domain/unix_domain_socket_address.h) copies at most
  `sizeof(sun_path) - 1 - 1` bytes (108 - 1 null slot - 1 abstract-prefix byte = **106 bytes** on Linux)
  via `memcpy`, with **no bounds error and no truncation warning**. Two different identifiers that share
  the same first 106 bytes silently collide on the same abstract address.
- **QNX native messaging**:
  [`QnxResourcePath`](../../qnx_dispatch/qnx_resource_path.h) enforces
  `kMaxIdentifierLen = 256` via `SCORE_LANGUAGE_FUTURECPP_PRECONDITION` in
  [qnx_resource_path.cpp](../../qnx_dispatch/qnx_resource_path.cpp), which **panics/aborts the process**
  if violated (also rejects an empty identifier, which Unix Domain silently accepts).

Consequences / gotchas:
- An identifier that is valid and unique on the QNX target (up to 256 bytes) can silently collide with
  another service when the same code is exercised against the Unix Domain Sockets backend (used for host
  testing and, per `IServerConnectionGetClientIdentityAPI`'s note, "for internal testing purposes only" on
  QNX itself).
- The failure mode differs by backend: silent misbehavior (Unix Domain) vs. hard contract-violation crash
  (QNX). Neither is surfaced through `IClientFactory::Create` / `IServerFactory::Create`'s
  `noexcept`/`score::cpp::expected`-based error reporting.
- Nothing in the public headers tells an integrator what the safe/portable maximum is. **106 bytes is the
  practical cross-platform ceiling today**, driven entirely by an implementation detail of the Linux
  backend.

## 2. Preferred `identifier` size to avoid heap fragmentation (short string optimization)

`identifier` is stored as `score::cpp::pmr::string` (i.e.
`std::basic_string<char, char_traits<char>, polymorphic_allocator<char>>`, see
[string.hpp](../../../../../eclipse-score-baselibs/score/language/futurecpp/include/score/string.hpp)) in
both `ClientConnection::identifier_` and `UnixDomainServer`/`QnxDispatchServer::identifier_`. Whether
storing the identifier allocates from the supplied `memory_resource` at all depends entirely on the
underlying standard library's short-string-optimization (SSO) capacity, which is implementation-defined:

- libstdc++ (GCC, used on Linux/host builds): **15 bytes** inline capacity.
- libc++: 22 bytes inline capacity.

Today this is not documented anywhere near `ServiceProtocolConfig::identifier`, so there is no guidance
telling integrators that keeping identifiers to **≤ 15 bytes** avoids an extra heap allocation (and the
resulting fragmentation/non-determinism) per `ClientConnection`/`Server` instance, on the most conservative
(and currently used) standard library. Anything above that is not wrong, just costs one extra allocation
from the configured `memory_resource` per instance.

Recommendation (not yet written as a requirement): document a **preferred maximum of 15 bytes** for
`identifier` next to the field, distinct from the **hard portability ceiling of 106 bytes** from finding 1.

## 3. No rough memory-usage model is specified per configuration / connection count

None of `ServiceProtocolConfig`, `ClientConfig`, or `ServerConfig` document the memory consequence of the
counts they configure. Based on reading the current implementation:

### Client side (`ClientConnection`, [client_connection.cpp](../../client_connection.cpp))

Per `ClientConnection` instance, from the supplied `memory_resource`:
- `identifier_`: 0 or 1 allocation of `identifier.size() + 1` bytes (see findings 1 & 2).
- `callback_context_` (`score::cpp::pmr::make_shared<CallbackContext>`): 1 allocation (control block +
  `recursive_mutex` + the user's `StateCallback`; the user callback's own captured state can add further,
  unaccounted allocations if it exceeds `std::function`'s small-object buffer).
- `send_storage_`: 1 allocation for the backing array of
  `max_queued_sends + max_async_replies` `SendCommand` elements, **plus one allocation per element**
  (`message.reserve(max_send_size)` in the constructor, see
  [client_connection.cpp](../../client_connection.cpp)). This matches the
  `ClientConnectionSendQueuePreallocation` requirement (bounded, monotonic, at construction time) — this
  part *is* correctly documented in `component_requirements.trlc`, but the resulting byte count is not.

  Rough order of magnitude: `(max_queued_sends + max_async_replies) * (sizeof(SendCommand) + max_send_size)`.

Amortized/shared across *all* connections created via the same engine (not per connection — see finding 5):
the engine's single receive buffer, sized to the largest `max(max_reply_size, max_notify_size)` registered
by any connection sharing that engine.

### Server side (`UnixDomainServer`/`QnxDispatchServer`)

Per accepted connection (see finding 4 for *when* this happens):
- Unix Domain ([unix_domain_server.cpp](../../unix_domain/unix_domain_server.cpp)): 1 allocation for the
  `ServerConnection` object only. `Reply`/`Notify` write directly to the socket with no library-side
  buffering, so `max_reply_size`/`max_notify_size` do not translate into extra per-connection memory on
  this backend today.
- QNX ([qnx_dispatch_server.cpp](../../qnx_dispatch/qnx_dispatch_server.cpp)): 1 allocation for the
  `ServerConnection` object, **plus** `reply_message_.message.reserve(max_reply_size)` (1 allocation), plus
  `notify_storage_` sized to `max_queued_notifies` elements (1 allocation for the array, plus one per
  element reserved to `max_notify_size` bytes).

  Rough order of magnitude (QNX): `sizeof(ServerConnection) + max_reply_size + max_queued_notifies * max_notify_size`.

None of this is written down next to `ServerConfig`/`ClientConfig`/`ServiceProtocolConfig`. An integrator
sizing a `memory_resource` (e.g. a `monotonic_buffer_resource` for an ASIL-B process) currently has to read
the implementation to derive these formulas.

## 4. `ServerConfig::pre_alloc_connections` and `ServerConfig::max_queued_sends` have no effect (requirement vs. implementation gap)

[i_server_factory.h](../../i_server_factory.h) documents:
- `pre_alloc_connections`: "Number of preallocated server connections. 0 if there is no preallocation
  (fine for QM apps, but bad for monotonic memory allocation)".
- `max_queued_sends`: "Maximum number of Send messages by clients queued on server side.".

`component_requirements.trlc` additionally states two **ASIL B** requirements built on these fields:
`ServerPreallocatesConnectionObjects` ("preallocate memory for the number of `IServerConnection` objects
... at construction time, without allocating additional memory for each incoming client connection") and
`ServerRingBufferQueueSizeConfigurable` ("implement the shared incoming message queue as a ring buffer with
the number of slots equal to `ServerConfig::max_queued_sends`").

However, in both backends today:
- `UnixDomainServer`'s constructor ([unix_domain_server.cpp](../../unix_domain/unix_domain_server.cpp))
  takes `ServerConfig` as an **unused, unnamed parameter** (`const IServerFactory::ServerConfig&
  /*server_config*/`). `pre_alloc_connections` and `max_queued_sends` are not read at all.
- `QnxDispatchServer` stores `server_config_` but never reads `pre_alloc_connections`, and its own
  constructor comments `max_request_size_` as `// currently unused`
  ([qnx_dispatch_server.cpp](../../qnx_dispatch/qnx_dispatch_server.cpp)).
- Every `ServerConnection` (both backends) is heap-allocated on demand in the connect path
  (`score::cpp::pmr::make_unique<ServerConnection>(...)` in `ProcessConnect`/`AcceptConnection`), i.e. **at
  accept time**, not preallocated at server construction time.
- Neither backend implements an explicit ring buffer for incoming messages; queuing of not-yet-processed
  `Send`/`SendWaitReply` messages is effectively delegated to the OS (socket receive buffer / QNX channel),
  which is not sized from `max_queued_sends` anywhere in this code.

This means two existing ASIL B requirements are currently **not satisfied by either implementation**, and
`pre_alloc_connections`/server-side `max_queued_sends` are effectively dead configuration fields. This is
a pre-existing gap independent of the identifier/memory-sizing topic above, but it directly affects any
memory-usage model for the server side (finding 3), since "preallocated at construction" would materially
change the formulas. Recorded here as technical debt to track and fix (or to consciously retire the two
requirements) before writing any new sizing requirements that assume preallocation happens.

## 5. Undocumented but useful property: the receive buffer is per-engine, not per-connection

Both engines maintain a single, monotonically-growing receive buffer shared by every connection registered
on that engine (`posix_receive_buffer_` in
[unix_domain_engine.cpp](../../unix_domain/unix_domain_engine.cpp) and
[qnx_dispatch_engine.cpp](../../qnx_dispatch/qnx_dispatch_engine.cpp)), sized to the **largest**
`max_receive_size` registered by any endpoint on that engine — not `O(connections)`. This is a favorable
property for the memory model in finding 3 (it bounds the "reception" side cost to a single buffer per
engine/process rather than per connection), but it is currently discoverable only by reading the engine
implementations. Worth stating explicitly once the memory-usage model is written down as a requirement, so
integrators don't over-budget for it.

## 6. Missing test coverage: proving containment within the supplied `memory_resource`

There is currently no test (client or server, either backend) that asserts allocations made by
`ClientConnection`/`UnixDomainServer`/`QnxDispatchServer` are drawn exclusively from the `memory_resource`
returned by `ISharedResourceEngine::GetMemoryResource()`, as opposed to silently falling back to
`score::cpp::pmr::get_default_resource()` (the process-wide default, normal heap unless overridden) when a
code path forgets to thread the resource through. All existing tests
(e.g. [client_connection_test.cpp](../../client_connection_test.cpp),
[unix_domain_server_test.cpp](../../unix_domain_server_test.cpp)) use
`score::cpp::pmr::get_default_resource()` as the "custom" resource, which cannot distinguish "used the
supplied resource" from "used the process default" — both are the same object.

Proposed (not yet implemented) approach for future tests:
1. Install a small `memory_resource` that fails the test (e.g. `ADD_FAILURE()`) on any `do_allocate` call,
   as the process-wide default via `score::cpp::pmr::set_default_resource(...)` (save/restore the previous
   default around the test).
2. Construct the `ClientConnection`/`Server` (or their factories) with a *different*, explicitly-supplied
   counting `memory_resource`.
3. Assert the poisoned default resource was never touched, and (to make sure the instrumentation itself is
   exercised) that the supplied counting resource recorded at least one allocation, e.g. by using an
   `identifier` longer than the SSO capacity from finding 2 and/or a non-zero queue count.
4. Caveat to document alongside such tests: this only proves containment at the C++ allocator level. The
   OS itself still draws from its own per-connection kernel resources for the underlying transport (e.g.
   Unix Domain socket buffers, QNX channel/connection tables), which are outside the scope of
   `memory_resource` and cannot be redirected to a user-supplied arena.

This is left as future work; no test code has been added for this yet.

## Suggested follow-up (not done here)

- Turn findings 1 and 2 into a documented contract on `ServiceProtocolConfig::identifier` (hard max across
  supported backends + a preferred size to stay within SSO), most likely with a corresponding
  `CompReq`/note in `component_requirements.trlc` and a shared constant both backends can reference/assert
  against consistently (today `106` only exists implicitly as `sizeof(sun_path) - 2`, and `256` only exists
  as `QnxResourcePath::kMaxIdentifierLen`).
- Decide whether `ServerConfig::pre_alloc_connections` and server-side `max_queued_sends` (finding 4) should
  be implemented to match the existing ASIL B requirements, or the requirements should be revised to match
  reality; either way, this should probably be resolved before formalizing the memory-usage model from
  finding 3 as a requirement.
- Once the above is settled, write down the rough per-connection memory formulas from finding 3 as
  documentation (and/or requirements), and add the containment tests sketched in finding 6.
