# Service Discovery Message-Passing Server Unit Design

## Unit Purpose

This unit hosts the daemon over message passing and bridges transport callbacks to daemon operations.
It manages subscriber-session state for asynchronous availability notifications.

## Responsibilities

1. Start and stop the message-passing server endpoint.
2. Build provider context from transport identity data.
3. Route request payloads to daemon handlers and return serialized replies.
4. Allocate and retire search handles for StartFindService and StopFindService.
5. Notify subscribers when relevant service keys change.

## Interfaces

- service_discovery_message_passing_server.h
- service_discovery_message_passing_server.cpp

## Safety-Relevant Behavior

1. Maintain consistent mapping between session identity and search ownership.
2. Remove subscriber entries on disconnect.
3. Prevent notification dispatch to inactive or stale subscriber entries.

## Data Ownership

1. Daemon instance is injected and not owned by this unit.
2. Subscriber table is fixed-size and session-scoped.

## Test Seams

Primary verification uses service_discovery_message_passing_integration_test.cpp and daemon disconnect test scenarios.
