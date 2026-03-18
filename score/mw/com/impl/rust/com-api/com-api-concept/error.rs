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

pub use com_api_concept_macros::ErrorDisplay;

/// Detailed information about Service Discovery failure reasons,
/// including specific issues with service interfaces, instance specifiers, and handle retrieval.
#[derive(ErrorDisplay)]
pub enum ServiceDiscoveryFailedReason {
    #[error = "Invalid instance specifier format or content, which may not be according to the expected format or contain invalid content"]
    InstanceSpecifierInvalid,
    #[error = "Service not found, which may not be available currently or accessible"]
    ServiceNotFound,
    #[error = "Internal error during service discovery"]
    InternalError(&'static str),
}

/// Reason for producer creation failure
#[derive(ErrorDisplay)]
pub enum ProducerCreationReason {
    #[error = "Invalid instance specifier format or content, which may not be according to the expected format or contain invalid content"]
    InstanceSpecifierInvalid,
    #[error = "Skeleton creation failed"]
    SkeletonCreationFailed,
    #[error = "Builder creation failed for producer instance"]
    BuilderCreationFailed,
}

/// Reason for consumer creation failure
#[derive(ErrorDisplay)]
pub enum ConsumerCreationReason {
    #[error = "Invalid instance specifier format or content, which may not be according to the expected format or contain invalid content"]
    InstanceSpecifierInvalid,
    #[error = "Service not found from service discovery handle"]
    ServiceHandleNotFound,
    #[error = "Proxy creation failed"]
    ProxyCreationFailed,
}

/// Reason for subscribe operation failure
#[derive(ErrorDisplay)]
pub enum SubscribeFailureReason {
    #[error = "Event not available for subscription, possibly due to missing event type or incompatible service"]
    EventNotAvailable,
    #[error = "Internal subscription error"]
    InternalError(&'static str),
}

/// Memory allocation error details
#[derive(ErrorDisplay)]
pub enum AllocationFailureReason {
    #[error = "Requested size exceeds available memory which is configured during subscription setup"]
    OutOfMemory,
    #[error = "Invalid allocation request"]
    InvalidRequest,
    #[error = "Internal allocation error related to allocation operations of shared memory or sample container"]
    InternalError(&'static str),
}

/// Comprehensive error reasons asynchronous receive failure
#[derive(ErrorDisplay)]
pub enum AsyncReceiveFailedReason {
    #[error = "Error at the time of initialzing async receive operation"]
    InitializationFailed,
    #[error = "Receive operation failed because of internal error"]
    ReceiveError,
    #[error = "Internal receive buffer not available for receive operation"]
    BufferUnavailable,
    #[error = "Sample size out of bounds, expected at most {}, but got {}"]
    SampleCountOutOfBounds(usize, usize),
}

/// Comprehensive error reasons for receive failure
#[derive(ErrorDisplay)]
pub enum ReceiveFailedReason {
    #[error = "Error at the time of initializing receive operation"]
    InitializationFailed,
    #[error = "Receive operation failed because of internal error"]
    ReceiveError,
    #[error = "Sample size out of bounds, expected at most {}, but got {}"]
    SampleCountOutOfBounds(usize, usize),
}

/// Comprehensive error reasons for event-related failures
#[derive(ErrorDisplay)]
pub enum EventFailedReason {
    #[error = "Event creation error, possibly due to invalid event type or internal error"]
    EventCreationFailed,
    #[error = "Event publication failed, possibly due to internal error"]
    EventPublishFailed,
}

/// Error enumeration for different failure cases in the Consumer/Producer/Runtime APIs.
#[derive(ErrorDisplay)]
pub enum Error {
    #[error = "Service discovery operation failed due to"]
    ServiceDiscoveryFailed(ServiceDiscoveryFailedReason),
    #[error = "Producer creation failed due to"]
    ProducerCreationFailed(ProducerCreationReason),
    #[error = "Consumer creation failed due to"]
    ConsumerCreationFailed(ConsumerCreationReason),
    #[error = "Failed to offer service instance for discovery"]
    OfferServiceFailed,
    #[error = "Failed to send sample data to the service instance, possibly due to internal error"]
    SendingDataFailed,
    #[error = "Event operation failed due to"]
    EventError(EventFailedReason),
    #[error = "Sample container allocation failed due to"]
    SampleContainerAllocateFailed(AllocationFailureReason),
    #[error = "Memory allocation failed due to"]
    MemoryAllocationFailed(AllocationFailureReason),
    #[error = "Asynchronous receive operation failed due to"]
    AsyncReceiveFailed(AsyncReceiveFailedReason),
    #[error = "Receive operation failed due to"]
    ReceiveFailed(ReceiveFailedReason),
    #[error = "Invalid instance specifier provided, which may be not according to the expected format or contain invalid content"]
    InstanceSpecifierInvalid,
    #[error = "Event subscription failed due to"]
    SubscribeFailed(SubscribeFailureReason),
}
