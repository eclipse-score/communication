# Service Discovery Registry Unit Design

## Unit Purpose

This unit stores and resolves service registrations and lock ownership state.
It is the authoritative in-memory model used by the daemon for registration lifecycle decisions.

## Responsibilities

1. Register and unregister service offers using service key and provider context.
2. Validate integrity claims and ownership constraints.
3. Provide creation-lock and usage-lock semantics for service instances.
4. Remove all session-owned state on disconnect.
5. Resolve registrations for lookup and notification generation.

## Interfaces

- i_service_discovery_registry.h
- service_discovery_registry.h
- service_discovery_registry.cpp

## Safety-Relevant Behavior

1. Reject integrity escalation and unauthorized unregister requests.
2. Keep lock ownership and registration ownership bound to provider session identity.
3. Return deterministic lookup sets for a given registry state.

## Data Structures

1. Service map keyed by service key.
2. Creation lock map keyed by service key with exclusive owner session.
3. Usage lock map keyed by service key with shared and exclusive lock state.

## Test Seams

Primary verification uses service_discovery_registry_test.cpp with ownership, lock, and disconnect cleanup scenarios.
