# Service Discovery Daemon Unit Design

## Unit Purpose

This unit implements daemon-side request dispatch and response generation.
It maps decoded protocol operations to registry actions and response status codes.

## Responsibilities

1. Deserialize request payloads and validate operation type.
2. Dispatch operations to registry methods with provider context.
3. Serialize operation results into protocol responses.
4. Process disconnect callbacks by removing session-owned state.

## Interfaces

- service_discovery_daemon.h
- service_discovery_daemon.cpp

## Safety-Relevant Behavior

1. Avoid partial processing for malformed inputs.
2. Ensure status codes reflect operation outcomes for caller handling.
3. Ensure disconnect processing removes owned registry entries and locks.

## Operational Assumptions

The daemon runs as single active registry authority and receives session identity from the message-passing server adapter.

## Test Seams

Primary verification uses service_discovery_daemon_test.cpp with per-operation and disconnect handling scenarios.
