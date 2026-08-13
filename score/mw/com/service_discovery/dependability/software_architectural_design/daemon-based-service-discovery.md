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

The current implementation provides:

- A thread-safe in-memory registry with ownership checks.
- Binary protocol encoding and decoding for daemon requests and responses.
- A daemon request handler that applies protocol operations against the registry.
- A message-passing server and client adapter pair that carries the daemon protocol over the platform IPC layer.
- A compatibility facade that preserves the current IServiceDiscovery entry points while mapping them onto the daemon-backed client.

The daemon protocol and registry also support start-find and stop-find operations, lock ownership semantics for service instances,
and deterministic per-request response ordering for batch-compatible operation handling.

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

- Session-bound cleanup
: Registrations, subscriptions, and lock ownership are removed when the owning daemon session disconnects.

- Stale entry supervision
: Liveness supervision requirements are modeled and tied to monotonic time assumptions.

## Migration Plan Hooks

- Keep compatibility facade as the runtime integration boundary while daemon protocol evolves.
- Preserve a single daemon authority per system configuration.
- Keep test coverage for register, resolve, find-service notifications, and lock cleanup on disconnect as release-gate evidence.
- Extend supervision and policy configuration test depth before moving maturity from development to release.

## Deferred Capabilities (Development Maturity)

The following capabilities are intentionally deferred while maturity remains set to development.
These are tracked as existing component requirements and become release-gate obligations once implemented.

1. ServiceDiscovery.StaticOfferAuthorizationLoadedAtStartup
Status: Deferred
Reason: Daemon startup policy-ingestion path is not implemented yet.
Activation trigger: Add startup policy loading and verify with positive/negative authorization tests.

2. ServiceDiscovery.RegisterRejectsUnauthorizedOfferer
Status: Deferred
Reason: Depends on active static offer policy configuration in daemon runtime.
Activation trigger: Enforce UID authorization in register flow and add rejection behavior coverage.

3. ServiceDiscovery.StaleEntriesAreRemoved
Status: Deferred
Reason: Liveness supervision loop and expiry execution policy are not implemented yet.
Activation trigger: Implement monotonic-time supervision cycle and expiry-based removal tests.

4. ServiceDiscovery.BatchedOperationsPreservePerCallResults
Status: Deferred
Reason: End-to-end batch request execution path is not implemented yet.
Activation trigger: Add batch processing pipeline and ordered per-call result verification.

5. ServiceDiscovery.ProtocolEncodesBatchRequests
Status: Deferred
Reason: Protocol currently covers single-operation request-reply exchanges.
Activation trigger: Add operation-list payload encoding/decoding and parser interoperability tests.

6. ServiceDiscovery.AuthorizationPolicyConfigurationApi
Status: Deferred
Reason: Runtime-facing configuration API for policy handover is not exposed yet.
Activation trigger: Add configuration API and startup integration tests proving policy availability before offers.
