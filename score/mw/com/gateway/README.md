# LoLa inter-domain Gateway

## Overview

The gateway enables `mw::com`/`LoLa`-based communication across distinct `LoLa` domains. A `LoLa` domain is a set
of applications sharing the same OS instance that communicate using `LoLa` proxies and skeletons.

Typical scenarios where the gateway applies:
- `mw::com` communication between SoCs interconnected via a physical link (e.g. PCIe, USB4)
- `mw::com` communication between VMs in a hypervisor setup (guest–guest, guest–host)

The conceptual design for the generalized gateway is documented at:
[Concept for a generalized LoLa Gateway](../dependability/software_architectural_design/generalized_gateway/README.md)

This README describes the implementation details specific to this codebase.

## Code Map

| File | Role |
|---|---|
| [gateway_application/gateway_application.h](gateway_application/gateway_application.h) / [.cpp](gateway_application/gateway_application.cpp) | `GatewayApplication` — central implementation; acts as both `GatewayCore` (destination side) and orchestrator (source side). |
| [gateway_application/gateway_core.h](gateway_application/gateway_core.h) | `GatewayCore` interface — called by the transport layer to drive local actions on the destination side. |
| [gateway_application/gateway_error.h](gateway_application/gateway_error.h) | Error codes returned by `GatewayApplication`. |
| [gateway_application/configuration/gateway_configuration.h](gateway_application/configuration/gateway_configuration.h) | Parsed gateway configuration (forwarded/received services, transport ID, config path). |
| [gateway_application/configuration/gateway_config_parser.h](gateway_application/configuration/gateway_config_parser.h) | Parses `mw_com_gateway_config.json` into `GatewayConfiguration`. |
| [transport_layer/transport.h](transport_layer/transport.h) | `Transport` abstract base class — called by `GatewayApplication` to send cross-domain requests. |
| [transport_layer/transport_factory.h](transport_layer/transport_factory.h) | Creates the correct `Transport` implementation from configuration. |
| [transport_layer/transport_mock.h](transport_layer/transport_mock.h) | GMock of `Transport` used in unit tests. |
| [transport_layer/sample/](transport_layer/sample/) | Sample transport layer implementation: relies on shared memory in a hypervisor system and uses TCP sockets for side-channel communication. |

## Architecture

Each `LoLa` domain runs a gateway process instance. Together they form a single logical gateway — one process per
domain — with a transport layer connecting them.

**Source Gateway**: The gateway process in the domain where a service instance is originally provided. It creates a
`GenericProxy` to consume the service locally and forwards service availability and event update notifications to
the other domain via the transport layer.

**Destination Gateway**: The gateway process in the domain that receives the forwarded service. It creates a
`GenericSkeleton` (Forwarding Skeleton) as a local substitute for the original service and offers it to local
consumers.

> **Note:** Source and Destination are roles, not fixed processes. A single `GatewayApplication` instance can
> act as Source for some services and Destination for others simultaneously.

### Transport Layer

The transport layer is abstracted via the `Transport` base class in
[transport_layer/transport.h](transport_layer/transport.h). Different implementations are plugged in depending on
the cross-domain setup (socket-based, shared-memory, etc.) and are selected at runtime by
`TransportFactory::Create()` using the transport layer ID from the gateway configuration.

### Gateway Core

The `GatewayCore` interface is the inbound API the transport layer calls on the **Destination Gateway** side to
drive local actions: creating/offering/stopping `Forwarding Skeleton`s and managing event update notification
registrations. `GatewayApplication` implements this interface.

### Gateway Types

| Type | Determined by | Description |
|---|---|---|
| **Copying Gateway** | `Transport::IsMemorySharingSupported()` returns `false` | Copies event data between domains via the transport layer. |
| **MemorySharing Gateway** | `Transport::IsMemorySharingSupported()` returns `true` | Shares the source domain's memory objects directly with the destination domain, avoiding data copying. |

## Configuration

### Required Config Files (both sides)

Every gateway process — regardless of whether it acts as Source or Destination — needs **three** configuration
files in its domain:

1. `mw_com_gateway_config.json` — gateway-specific config (forwarded/received services, transport selection).
   Parsed by `GatewayConfigParser` and consumed by `GatewayApplication::Setup()`. See its
   [schema](gateway_application/configuration/mw_com_gateway_config_schema.json) and
   [example](gateway_application/configuration/example/mw_com_gateway_config.json).
2. The transport-layer config file referenced by the `config-path` field of the `transport-layer` section in
   `mw_com_gateway_config.json`. Its content is transport-specific and is parsed during
   `GatewayApplication::Setup()` by the selected transport (via `TransportFactory::Create()`). For the bundled
   `sample_hypervisor` transport this is the hypervisor-socket config (remote IP, local/remote ports, request
   timeout) — see its
   [schema](transport_layer/sample/configuration/mw_com_gateway_sample_transport_config_schema.json) and
   [example](transport_layer/sample/configuration/example/mw_com_gateway_sample_transport_config.json).
3. `mw_com_config.json` — standard `mw::com` deployment config (provided as for all `mw::com` applications).
   Required by the `mw::com` runtime to resolve the `InstanceSpecifier`s referenced in
   `mw_com_gateway_config.json` into concrete deployment info (binding, number of slots, ASIL level, allowed
   consumers).

How each side uses `mw_com_config.json`:
- **Source side** — `GatewayApplication::Start()` creates a `GenericProxy` (via `StartFindService()`) for each
  entry in `forwarded-services`; the runtime reads `mw_com_config.json` to look up the deployment of those
  instances.
- **Destination side** — `GenericSkeleton::Create()` reads `mw_com_config.json` to look up the deployment of each
  forwarded service instance it has to provide locally. **This file must list every forwarded service instance on
  the destination side**, otherwise skeleton creation fails.

### `mw_com_gateway_config.json` fields

Key fields:

- `forwarded-services`: `InstanceSpecifier`s of services provided locally that this gateway shall forward to the
  other domain.
- `expected-received-services`: `InstanceSpecifier`s of services expected from the other domain. This is an
  **active safety check** — `GatewayApplication::ProvideService()` rejects (returns
  `GatewayErrorc::kNonWhitelistedService`) any incoming service not listed here.
- `transport-layer`: selects the transport layer implementation (by `id`) and points to its dedicated config file
  (via `config-path`).

See [gateway_application/configuration/](gateway_application/configuration/) for the JSON schema and examples.

## Detailed Sequences

---

### Initialization

1. On both the source and destination side, `GatewayApplication::Setup()` calls
   `TransportFactory::Create(GatewayCore&, transport_id, config_path)` to instantiate the configured `Transport`
   implementation.
2. `Transport::Setup()` is called on both sides to establish the cross-domain connection (e.g. open sockets, map
   shared memory regions).
3. On the source side, `GatewayApplication::Start()` calls `StartServiceDiscovery()`, which calls
   `GenericProxy::StartFindService()` for each entry in `forwarded-services`.

---

### Service Provision

Triggered when a locally provided service is discovered by the Source Gateway.

1. On the source side, `FindServiceHandler` fires — a service matching a `forwarded-services` entry has appeared.
2. On the source side, `GatewayApplication` creates a `GenericProxy` with the received handle.
3. On the source side, from the `GenericProxy`, it retrieves the name and data-type info (`impl::EventInfo`) of
   all events via `GenericProxy::GetEvents()`.
4. On the source side, `GatewayApplication` calls `Transport::ProvideService(InstanceSpecifier,
   vector<impl::EventInfo>)`.
5. The transport layer serializes and sends a `ProvideService` message carrying the `InstanceSpecifier` and event
   metadata to the destination gateway.
6. On the destination side, the transport layer performs any cross-domain setup required (e.g. mapping
   shared-memory objects for a MemorySharing gateway, or preparing a data path for a Copying gateway).
7. On the destination side, the transport layer calls `GatewayCore::ProvideService(InstanceSpecifier,
   vector<impl::EventInfo>)`.
8. On the destination side, `GatewayApplication` checks that the `InstanceSpecifier` is in
   `expected-received-services`. If not, it returns `GatewayErrorc::kNonWhitelistedService` immediately.
9. On the destination side, if a skeleton for this `InstanceSpecifier` **already exists** (service bounced and
   reappeared), `GatewayApplication` reuses the existing `GenericSkeleton` — it calls `OfferService()` on it and
   re-registers active event subscriptions, then returns. No new skeleton is created. This avoids zeroing the
   shared-memory `EventSubscriptionControl` while local consumers hold active subscriptions.
10. On the destination side, otherwise `GatewayApplication` calls `GenericSkeleton::Create(InstanceSpecifier,
    GenericSkeletonServiceElementInfo)`. The skeleton reads deployment info from `mw_com_config.json`. It either
    reuses source-domain shared-memory objects (MemorySharing) or creates its own (Copying).
11. On the destination side, on success, a per-event callback is registered via
    `GenericSkeletonEvent::SetReceiveHandlerNotificationCallback` so that subscriber changes are forwarded back to
    the source gateway.
12. The result of `ProvideService()` is returned from the destination gateway back to the source gateway.
13. On the source side, on success, `GatewayApplication` calls `Transport::OfferService()`. On failure, it logs
    the error and removes the proxy — the service will be retried on the next `FindServiceHandler` invocation.
14. The `OfferService` message is forwarded to the destination gateway.
15. On the destination side, `GatewayApplication` calls `GenericSkeleton::OfferService()`, making the forwarded
    service visible to local consumers.

---

### StopOfferService

Triggered when a previously forwarded service disappears on the source side.

1. On the source side, `FindServiceHandler` fires with an empty handle list — the service has gone.
2. On the source side, `GatewayApplication` calls `Transport::StopOfferService(InstanceSpecifier)`.
3. The transport layer forwards a `StopOfferService` message to the destination gateway.
4. On the destination side, the transport layer calls `GatewayCore::StopOfferService(InstanceSpecifier)`.
5. On the destination side, `GatewayApplication` calls `GenericSkeleton::StopOfferService()`.
   > **Important:** The `GenericSkeleton` instance is **kept alive** (not destroyed). Destroying it would
   > zero the shared-memory `EventSubscriptionControl`, crashing any local consumer proxy that still holds an
   > active subscription. The skeleton is reused if the service reappears (see step 9 of Service Provision).

---

### OfferService

Triggered as the final step of [Service Provision](#service-provision) and again on every subsequent
re-appearance of a service that was already forwarded.

1. On the source side, `GatewayApplication` calls `Transport::OfferService(InstanceSpecifier)`.
2. The transport layer forwards an `OfferService` message to the destination gateway.
3. On the destination side, the transport layer calls `GatewayCore::OfferService(InstanceSpecifier)`.
4. On the destination side, `GatewayApplication` calls `GenericSkeleton::OfferService()`.
5. The outcome is returned from the destination gateway back to the source gateway.

---

### RegisterEventUpdateNotification

> **Note:** This sequence flows from the **destination gateway to the source gateway** — the reverse of all other
> sequences. It is triggered by a local consumer on the destination side subscribing to an event.

1. On the destination side, the callback registered via
   `GenericSkeletonEvent::SetReceiveHandlerNotificationCallback` fires — the first local consumer has registered
   an event receive handler.
2. On the destination side, `GatewayApplication` calls `Transport::RegisterUpdateNotification(InstanceSpecifier,
   ServiceElementType::EVENT, event_name)`.
3. The transport layer forwards a `RegisterUpdateNotification` message to the source gateway.
4. On the source side, the transport layer calls `GatewayCore::RegisterUpdateNotification(InstanceSpecifier,
   ServiceElementType::EVENT, event_name)`.
5. On the source side, `GatewayApplication` subscribes the `GenericProxyEvent` and registers a
   `SetReceiveHandler` callback. When new data arrives, this callback calls `Transport::NotifyUpdate()` to inform
   the destination gateway.
6. The outcome is returned from the source gateway back to the destination gateway.

---

### NotifyEventUpdate

Triggered when the source gateway detects new event data and the destination side has an active subscriber
(i.e. `RegisterEventUpdateNotification` was previously called for this event).

1. On the source side, the `SetReceiveHandler` callback registered in step 5 above fires — new event data
   arrived.
2. On the source side, `GatewayApplication` calls `Transport::NotifyUpdate(InstanceSpecifier,
   ServiceElementType::EVENT, event_name)`.
3. The transport layer forwards a `NotifyUpdate` message to the destination gateway.
4. On the destination side, the transport layer calls `GatewayCore::NotifyUpdate(InstanceSpecifier,
   ServiceElementType::EVENT, event_name)`.
5. On the destination side, `GatewayApplication` calls `GenericSkeletonEvent::Notify()` on the corresponding
   event of the `Forwarding Skeleton`, which triggers the message-passing update notification to all local
   consumers.
