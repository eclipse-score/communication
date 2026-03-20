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

use thiserror::Error;

/// Detailed information about Service Discovery failure reasons,
/// including specific issues with service interfaces, instance specifiers, and handle retrieval.
#[derive(Debug, Error)]
pub enum ServiceFailedReason {
    #[error("Invalid instance specifier format or content, which may not be according to the expected format or contain invalid content")]
    InstanceSpecifierInvalid,
    #[error("Service not found, which may not be available currently or accessible")]
    ServiceNotFound,
    #[error("Failed to offer service instance for discovery")]
    OfferServiceFailed,
    #[error("Failed to start asynchronous discovery")]
    FailedToStartDiscovery,
}

/// Reason for producer failure
#[derive(Debug, Error)]
pub enum ProducerFailedReason {
    #[error("Invalid instance specifier format or content, which may not be according to the expected format or contain invalid content")]
    InstanceSpecifierInvalid,
    #[error("Skeleton creation failed")]
    SkeletonCreationFailed,
    #[error("Builder creation failed for producer instance")]
    BuilderCreationFailed,
}

/// Reason for consumer failure
#[derive(Debug, Error)]
pub enum ConsumerFailedReason {
    #[error("Invalid instance specifier format or content, which may not be according to the expected format or contain invalid content")]
    InstanceSpecifierInvalid,
    #[error("Service not found from service discovery handle")]
    ServiceHandleNotFound,
    #[error("Proxy creation failed")]
    ProxyCreationFailed,
}

/// Memory allocation error details
#[derive(Debug, Error)]
pub enum AllocationFailureReason {
    #[error(
        "Requested size exceeds available memory which is configured during subscription setup"
    )]
    OutOfMemory,
    #[error("Invalid allocation request")]
    InvalidRequest,
    #[error("Failed to allocate sample in shared memory for skeleton event")]
    AllocationToSharedMemoryFailed,
}

/// Comprehensive error reasons for receive failure
#[derive(Debug, Error)]
pub enum ReceiveFailedReason {
    #[error("Error at the time of initializing receive operation")]
    InitializationFailed,
    #[error("Receive operation failed because of internal error")]
    ReceiveError,
    #[error("Internal receive buffer not available for receive operation")]
    BufferUnavailable,
    #[error("Sample size out of bounds, expected at most {max}, but got {requested}")]
    SampleCountOutOfBounds { max: usize, requested: usize },
}

/// Comprehensive error reasons for event-related failures
#[derive(Debug, Error)]
pub enum EventFailedReason {
    #[error("Event creation error, possibly due to invalid event type or internal error")]
    EventCreationFailed,
    #[error("Event publication failed, possibly due to internal error")]
    EventPublishFailed,
    #[error("Failed to send sample data to the service instance, possibly due to internal error")]
    SendingDataFailed,
    #[error("Event not available for subscription, possibly due to missing event type or incompatible service")]
    EventNotAvailable,
}

/// Error enumeration for different failure cases in the Consumer/Producer/Runtime APIs.
#[derive(Debug, Error)]
pub enum Error {
    #[error("Service error due to: {0}")]
    ServiceError(ServiceFailedReason),
    #[error("Producer error due to: {0}")]
    ProducerError(ProducerFailedReason),
    #[error("Consumer error due to: {0}")]
    ConsumerError(ConsumerFailedReason),
    #[error("Event operation failed due to: {0}")]
    EventError(EventFailedReason),
    #[error("Sample container allocation failed due to: {0}")]
    AllocateError(AllocationFailureReason),
    #[error("Receive operation failed due to: {0}")]
    ReceiveError(ReceiveFailedReason),
}
