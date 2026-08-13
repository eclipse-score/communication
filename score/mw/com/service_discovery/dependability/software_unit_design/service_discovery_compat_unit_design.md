# Service Discovery Compatibility Facade Unit Design

## Unit Purpose

This unit preserves the existing runtime-facing IServiceDiscovery behavior while delegating implementation to the daemon-backed message-passing client.
It isolates migration impact for existing proxy and skeleton integration paths.

## Responsibilities

1. Map existing IServiceDiscovery calls to daemon protocol operations through the client adapter.
2. Preserve expected runtime API shape and callback behavior for current callers.
3. Convert daemon responses and errors into runtime-facing result types.

## Interfaces

- service_discovery_compat.h
- service_discovery_compat.cpp

## Safety-Relevant Behavior

1. Do not bypass daemon-side ownership and integrity checks.
2. Preserve lifecycle semantics for find-service start and stop operations.
3. Propagate operation failures to callers without silent fallback.

## Test Seams

Primary verification uses service_discovery_compat_test.cpp with runtime mock dependencies and daemon-backed server setup.
