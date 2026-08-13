# Service Discovery Message-Passing Client Unit Design

## Unit Purpose

This unit adapts daemon protocol operations to the message-passing client connection API used by applications.
It owns request-reply interactions and asynchronous notification callback dispatch for StartFindService subscriptions.

## Responsibilities

1. Establish and stop message-passing client connection lifecycle.
2. Send protocol requests and decode replies.
3. Track subscription entries by search handle.
4. Dispatch asynchronous availability notifications to registered callbacks.

## Interfaces

- service_discovery_message_passing_client.h
- service_discovery_message_passing_client.cpp

## Safety-Relevant Behavior

1. Maintain bounded subscription storage.
2. Deactivate subscriptions on stop-find and disconnect conditions.
3. Return explicit operation success or failure to upper layers.

## Concurrency Model

1. Connection state synchronization via mutex and condition variable.
2. Subscription mutation protected by dedicated mutex.

## Test Seams

Coverage is exercised via service_discovery_compat_test.cpp and integration scenarios in service_discovery_message_passing_integration_test.cpp.
