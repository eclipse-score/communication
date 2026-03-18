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
/// including specific issues with service interfaces, instance specifiers, and handle retrieval.
#[derive(Debug)]
pub enum ServiceDiscoveryFailedReason {
    /// Invalid instance specifier format or content
    InstanceSpecifierInvalid,
    /// Service handle not found
    ServiceNotFound,
    /// Internal error
    InternalError(&'static str),
}

/// Reason for producer creation failure
#[derive(Debug)]
pub enum ProducerCreationReason {
    /// Invalid instance specifier format or content
    InstanceSpecifierInvalid,
    /// Skeleton creation failed
    SkeletonCreationFailed,
    /// Builder creation failed
    BuilderCreationFailed,
    /// Internal error
    InternalError(&'static str),
}

/// Reason for consumer creation failure
#[derive(Debug)]
pub enum ConsumerCreationReason {
    /// Invalid instance specifier format or content
    InstanceSpecifierInvalid,
    /// Service handle not found from discovery
    ServiceHandleNotFound,
    /// Proxy creation failed
    ProxyCreationFailed,
    /// Internal error
    InternalError(&'static str),
}

/// Reason for subscribe operation failure
#[derive(Debug)]
pub enum SubscribeFailureReason {
    /// Event not available
    EventNotAvailable,
    /// Internal subscription error
    InternalError(&'static str),
}
/// Memory allocation error details
#[derive(Debug)]
pub enum AllocationFailureReason {
    /// Requested size exceeds available memory
    OutOfMemory,
    /// Invalid allocation request
    InvalidRequest,
    /// Internal allocation error
    InternalError(&'static str),
}

/// Comprehensive error reasons asynchronous receive failure
#[derive(Debug)]
pub enum AsyncReceiveFailedReason {
    /// initialization failed
    InitializationFailed,
    /// receive operation failed
    ReceiveError,
    /// Bufffer not available for receive operation
    BufferUnavailable,
    /// sample count out of bounds
    SampleCountOutOfBounds(&'static str, usize, usize),
}

/// Comprehensive error reasons for receive failure
#[derive(Debug)]
pub enum ReceiveFailedReason {
    /// initialization failed
    InitializationFailed,
    /// receive operation failed
    ReceiveError,
    /// sample count out of bounds
    SampleCountOutOfBounds(&'static str, usize, usize),
}

/// Comprehensive error reasons for event-related failures
#[derive(Debug)]
pub enum EventFailedReason {
    /// Event creation failed
    EventCreationFailed,
    /// Event publication failed
    EventPublishFailed,
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
    OfferServiceFailed,
    /// Failed to send sample
    SendingDataFailed,
    /// Event Error
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
    InstanceSpecifierInvalid,
    /// Event subscription failed
    SubscribeFailed(SubscribeFailureReason),
}

impl std::fmt::Display for ServiceDiscoveryFailedReason {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::InstanceSpecifierInvalid => {
                write!(f, "invalid instance specifier format or content")
            }
            Self::ServiceNotFound => write!(f, "service handle not found"),
            Self::InternalError(msg) => write!(f, "internal error: {}", msg),
        }
    }
}

impl core::fmt::Display for ReceiveFailedReason {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            Self::InitializationFailed => write!(f, "initialization failed"),
            Self::ReceiveError => write!(f, "receive operation failed"),
            Self::SampleCountOutOfBounds(msg, requested, max) => {
                write!(f, "{}: requested {}, max {}", msg, requested, max)
            }
        }
    }
}
