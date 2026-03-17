/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/// Detailed information about Service Discovery failure reasons,
/// including specific issues with service interfaces, instance specifiers, timeouts, and handle retrieval.
#[derive(Debug)]
pub enum ServiceDiscoveryFailedReason {
    /// Invalid instance specifier format or content
    InstanceSpecifierInvalid(String),
    /// Service handle not found
    ServiceNotFound(String),
    /// Internal error with details
    InternalError(String),
}

/// Reason for producer creation failure
#[derive(Debug)]
pub enum ProducerCreationReason {
    /// Invalid instance specifier format or content
    InstanceSpecifierInvalid(String),
    /// Skeleton creation failed with details
    SkeletonCreationFailed(String),
    /// Internal error with details
    InternalError(String),
}

/// Reason for consumer creation failure
#[derive(Debug)]
pub enum ConsumerCreationReason {
    /// Invalid instance specifier format or content
    InstanceSpecifierInvalid(String),
    /// Service handle not found from discovery
    ServiceHandleNotFound(String),
    /// Proxy creation failed with details
    ProxyCreationFailed(String),
    /// Internal error with details
    InternalError(String),
}

/// Reason for subscribe operation failure
#[derive(Debug)]
pub enum SubscribeFailureReason {
    /// Event not available
    EventNotAvailable(String),
    /// Internal subscription error
    InternalError(String),
}
/// Memory allocation error details
#[derive(Debug)]
pub enum AllocationFailureReason {
    /// Requested size exceeds available memory
    OutOfMemory(String),
    /// Invalid allocation request
    InvalidRequest(String),
    /// Internal allocation error
    InternalError(String),
}

/// Comprehensive error reasons asynchronous receive failure
#[derive(Debug)]
pub enum AsyncReceiveFailedReason {
    /// initialization failed with details
    InitializationFailed(String),
    /// receive operation failed with details
    ReceiveError(String),
    /// sample count out of bounds
    SampleCountOutOfBounds(String),
}

/// Comprehensive error reasons for receive failure
#[derive(Debug)]
pub enum ReceiveFailedReason {
    /// initialization failed with details
    InitializationFailed(String),
    /// receive operation failed with details
    ReceiveError(String),
    /// sample count out of bounds
    SampleCountOutOfBounds(String),
}

/// Comprehensive error reasons for event-related failures
#[derive(Debug)]
pub enum EventFailedReason {
    /// Event creation failed with details
    CreationFailed(String),
    /// Event publication failed with details
    PublishFailed(String),
}

/// Error enumeration for different failure cases in the Consumer/Producer/Runtime APIs.
#[derive(Debug)]
pub enum Error {
    /// Generic failure with optional message
    Fail(Option<String>),
    /// Service discovery failed with detailed reason
    ServiceDiscoveryFailed(ServiceDiscoveryFailedReason),
    /// Failed to create producer instance
    ProducerCreationFailed(ProducerCreationReason),
    /// Failed to create consumer instance
    ConsumerCreationFailed(ConsumerCreationReason),
    /// Failed to offer service instance for discovery
    OfferServiceFailed(String),
    /// Failed to send sample
    SendingDataFailed(String),
    /// Event Error with details
    EventError(EventFailedReason),
    /// Sample container allocation failed
    SampleContainerAllocateFailed(AllocationFailureReason),
    /// Memory allocation failed
    MemoryAllocationFailed(AllocationFailureReason),
    /// Asynchronous receive operation failed
    AsyncReceiveFailed(AsyncReceiveFailedReason),
    /// Receive operation failed
    ReceiveFailed(ReceiveFailedReason),
    /// Invalid instance specifier provided
    InstanceSpecifierInvalid(String),
    /// Event subscription failed with details
    SubscribeFailed(SubscribeFailureReason),
}
