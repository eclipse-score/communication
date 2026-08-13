# Service Discovery Protocol Unit Design

## Unit Purpose

This unit defines the binary protocol contract used between service discovery clients and daemon server endpoints.
It translates typed request, response, and notification structures to and from bounded byte payloads.

## Responsibilities

1. Define operation identifiers for register, unregister, resolve, find-service, and lock workflows.
2. Serialize and deserialize protocol payloads deterministically.
3. Enforce payload size bounds for request, response, and notification messages.
4. Preserve search-handle and registration data integrity across encoding boundaries.

## Interfaces

- service_discovery_protocol.h
- service_discovery_protocol.cpp

## Safety-Relevant Behavior

1. Reject malformed or truncated payloads during deserialization.
2. Prevent buffer overrun by honoring maximum serialized payload sizes.
3. Maintain deterministic field ordering and fixed-width encoding.

## Error Handling

Serialization returns false when output capacity is insufficient.
Deserialization returns empty optional when payload validation fails.

## Test Seams

Primary verification uses service_discovery_protocol_test.cpp with round-trip serialization coverage and malformed-input checks.
