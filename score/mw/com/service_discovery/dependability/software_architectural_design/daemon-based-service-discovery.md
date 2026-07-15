# Daemon-Based Service Discovery

## Design Challenges

- Filesystem and inotify based discovery is fragile during process crashes and path management races.
- The discovery path must remain transport independent while using Linux Unix Domain Sockets and QNX native message passing.
- Safety constraints require integrity partitioning and identity-based ownership checks.
- The daemon must provide deterministic behavior for lookup and unregister operations.
- The current API surface still needs a compatibility layer so existing IServiceDiscovery users can migrate without a flag day.
- Proxies need asynchronous availability updates, not just one-shot lookup responses.

## General Approach

The service discovery functionality is extracted into a dedicated daemon library interface.
Providers (skeletons) and consumers (proxies) communicate with the daemon only through the message passing layer.
The daemon owns the authoritative registry state and answers register, unregister, and resolve operations.

Current implementation slices already provide:

- A thread-safe in-memory registry with ownership checks.
- Binary protocol encoding and decoding for daemon requests and responses.
- A daemon request handler that applies protocol operations against the registry.
- A message-passing server and client adapter pair that carries the daemon protocol over the platform IPC layer.
- A compatibility facade that preserves the current IServiceDiscovery entry points while mapping them onto the daemon-backed client.

## Component Decomposition

- ServiceDiscoveryDaemon
: Decodes requests, routes operation to registry, encodes responses.

- ServiceDiscoveryRegistry
: Owns service entries indexed by service key and enforces registration rules.

- Protocol codec
: Provides deterministic binary payload serialization for message passing transport.

- Message passing server/client adapters
: Bridge the daemon protocol over the platform IPC layer and dispatch asynchronous notifications to subscribers.

- Compatibility facade
: Preserves the current IServiceDiscovery API shape for existing callers while using the daemon-backed client internally.

## Behavioral Flows

### Register Flow

1. Skeleton sends register request with service key, endpoint, provider identity, and integrity.
2. Daemon validates integrity claim against provider integrity.
3. Registry stores the registration if unique for provider identity.
4. Daemon returns status code success or error.

### Resolve Flow

1. Proxy sends resolve request for service key.
2. Registry returns a consistent snapshot of matching registrations.
3. Daemon responds with endpoint list.

### Async Find Flow

1. Proxy requests subscription for a service key.
2. Message passing client registers a notification callback with the daemon-backed server path.
3. Daemon tracks the subscription and pushes updates whenever availability changes.
4. Proxy receives appearance and disappearance callbacks without polling.

### Unregister Flow

1. Skeleton sends unregister request with service key and provider identity.
2. Registry removes entry only when identity matches the original registration.
3. Daemon responds with success or ownership-mismatch error.

## Safety-Oriented Design Notes

- Integrity partitioning
: A lower integrity provider cannot claim a higher integrity offer.

- Ownership enforcement
: Unregister requests are accepted only from the registering identity.

- Deterministic lookup
: Resolve returns immutable per-request snapshots.

- Stale entry supervision
: Planned next slice introduces lease and liveness timeouts using monotonic time.

## Migration Plan Hooks

- Keep daemon library standalone first.
- Add message passing server adapter in next implementation step.
- Integrate proxy/skeleton runtime in a later step once protocol and daemon behavior are stabilized.
- Preserve the current IServiceDiscovery facade until the runtime migration can absorb the new subscription-oriented API.
