/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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

//! This crate defines the concepts and traits of the COM API. It does not provide any concrete
//! implementations. It is meant to be used as a common interface for different implementations
//! of the COM API, e.g., for different IPC backends.
//!
//! # API Design principles
//!
//! - We stick to the builder pattern down to a single service (TODO: Should this be introduced to the C++ API?)
//! - We make all elements mockable. This means we provide traits for the building blocks.
//!   We strive for enabling trait objects for mockable entities. (TODO: Inspect all consuming methods for adherence to this rule)
//! - We allow for the allocation of heap memory during initialization phase (offer, connect, ...)
//!   but prevent heap memory usage during the running phase. Any heap memory allocations during the
//!   run phase must happen on preallocated memory chunks.
//! - We support data provision on potentially uninitialized memory for efficiency reasons.
//! - We want to be type safe by deriving as much type information as possible from the interface
//!   description.
//! - We want to design interfaces such that they're hard to misuse.
//! - Data communicated over IPC need to be position independent.
//! - Data may not reference other data outside its provenance.
//! - We provide simple defaults for customization points.
//! - Sending / receiving / calling is always done explicitly.
//! - COM API does not enforce the necessity for internal threads or executors.
//!
//! # Lower layer implementation principles
//!
//! - We add safety nets to detect ABI incompatibilities
//!
//! # Supported IPC ABI
//!
//! - Primitives
//! - Static lists / strings
//! - Dynamic lists / strings
//! - Key-value
//! - Structures
//! - Tuples

use core::fmt::Debug;
use core::future::Future;
use core::ops::{Deref, DerefMut};
pub mod reloc;
pub use reloc::Reloc;
use std::collections::VecDeque;
use std::path::Path;

/// Error enumeration for different failure cases in the Consumer/Producer/Runtime APIs.
#[derive(Debug)]
pub enum Error {
    /// TODO: To be replaced, dummy value for "something went wrong"
    Fail,
    Timeout,
    AllocateFailed,
    SubscribeFailed,
}

/// Result type alias with std::result::Result using com_api::Error as error type
pub type Result<T> = core::result::Result<T, Error>;

/// A factory-like trait for constructing complex objects through a builder pattern.
///
/// This trait enables type-safe construction of objects by moving self during the build phase,
/// ensuring all required configuration steps are completed before object instantiation.
///
/// # Type Parameters
/// * `Output` - The type of object being constructed
pub trait Builder<Output> {
    /// Construct the output object from the current builder state.
    ///
    /// Consumes self to prevent reuse and ensure immutability of the constructed object.
    ///
    /// # Returns
    ///
    ///  A 'Result' containing the constructed object on success and an 'Error' on failure.
    /// TODO: Should this be &mut self so that this can be turned into a trait object?
    fn build(self) -> Result<Output>;
}

/// This represents the com implementation and acts as a root for all types and objects provided by
/// the implementation.
pub trait Runtime {
    /// ServiceDiscovery<I> types for Discovers available service instances of a specific interface
    #[cfg(feature = "iceoryx")]
    type ServiceDiscovery<I: Interface + Debug>: ServiceDiscovery<I, Self>;
    #[cfg(not(feature = "iceoryx"))]
    type ServiceDiscovery<I: Interface>: ServiceDiscovery<I, Self>;

    /// Subscriber<T> types for Manages subscriptions to event notifications
    #[cfg(feature = "iceoryx")]
    type Subscriber<T: Reloc + Send + Debug + 'static>: Subscriber<T, Self>;
    #[cfg(not(feature = "iceoryx"))]
    type Subscriber<T: Reloc + Send + Debug>: Subscriber<T, Self>;

    /// ProducerBuilder<I, P> types for Constructs producer instances for offering services
    type ProducerBuilder<I: Interface, P: Producer<Self, Interface = I>>: ProducerBuilder<
        I,
        P,
        Self,
    >;

    /// Publisher<T> types for Publishes event data to subscribers
    #[cfg(feature = "iceoryx")]
    type Publisher<T: Reloc + Send + Debug + 'static>: Publisher<T, Self>;
    #[cfg(not(feature = "iceoryx"))]
    type Publisher<T: Reloc + Send + Debug>: Publisher<T, Self>;

    /// ProviderInfo types for Configuration data for service producers instances
    type ProviderInfo: Send + Clone;

    /// ConsumerInfo types for Configuration data for service consumers instances
    type ConsumerInfo: Send + Clone;

    /// Find a service instance for the given interface and instance specifier.
    /// Locate available instances of a service interface.
    ///
    /// Discovers service instances matching the provided instance specifier through
    /// the runtime's service discovery mechanism.
    ///
    /// # Parameters
    /// * `instance_specifier` - Target service instance identifier; use `InstanceSpecifier::MATCH_ANY`
    ///   to discover all available instances
    ///
    /// # Returns
    /// Service discovery handle for querying available instances
    #[cfg(feature = "iceoryx")]
    fn find_service<I: Interface + Debug>(
        &self,
        instance_specifier: FindServiceSpecifier,
    ) -> Self::ServiceDiscovery<I>;
    #[cfg(not(feature = "iceoryx"))]
    fn find_service<I: Interface>(
        &self,
        instance_specifier: FindServiceSpecifier,
    ) -> Self::ServiceDiscovery<I>;

    /// Create a producer builder for the given interface and producer type.
    /// Constructs a producer builder for offering services.
    ///
    /// # Parameters
    /// * `instance_specifier` - Unique identifier for this service instance; must not collide
    ///   with other offered instances of the same interface
    ///
    /// # Returns
    ///
    /// A configured builder ready for finalization via `build()`
    fn producer_builder<I: Interface, P: Producer<Self, Interface = I>>(
        &self,
        instance_specifier: InstanceSpecifier,
    ) -> Self::ProducerBuilder<I, P>;
}

/// Builder for Runtime instances with configuration support.
///
/// Extends the base builder pattern to support loading implementation-specific
/// configuration before runtime initialization.
///
/// # Type Parameters
/// * `B` - The runtime implementation being constructed
pub trait RuntimeBuilder<B>: Builder<B>
where
    B: Runtime,
{
    /// Load configuration from a specified file path.
    ///
    /// Reads implementation-specific configuration for the process which is going to offer the
    /// Services or events.
    ///
    /// # Parameters
    /// * `config` - Path to the configuration file
    ///
    /// # Returns
    ///
    /// A mutable reference to self for method chaining.
    fn load_config(&mut self, config: &Path) -> &mut Self;
}

/// Technology independent description of a service instance "location"
///
/// The string shall describe where to find a certain instance of a service. Each level shall look
/// like this
///  /my/path/to/service_name
/// validation for service name- /my/path/to/service_name
/// allowed characters: a-z A-Z 0-9 and '/'
/// Must start with leading/trailing check
/// Not allowed consecutive '/' characters
/// '_' is allowed in names
#[derive(Clone, Debug)]
pub struct InstanceSpecifier {
    specifier: String,
}

impl InstanceSpecifier {
    fn check_str(service_name: &str) -> bool {
        // Must start with exactly one leading slash
        if !service_name.starts_with('/') || service_name.starts_with("//") {
            return false;
        }

        // Remove the single leading slash
        let service_name = service_name.strip_prefix('/').unwrap();

        // Check each character
        let is_legal_char = |c| {
            (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
        };

        //validation of each path segment
        !service_name.is_empty()
            && service_name.split('/').all(|parts| {
                // No empty segments (reject trailing "/" and "//" in the middle)
                !parts.is_empty() && parts.chars().all(|c| is_legal_char(c))
            })
    }

    /// Create a new instance specifier, using the string-like input as the path to the
    /// instance.
    ///
    /// The returned instance specifier will only match if the instance exactly matches the given
    /// string.
    /// # Parameters
    /// * `service_name` - The string representing the instance path
    ///
    /// # Returns
    /// A 'Result' containing the constructed InstanceSpecifier on success and an 'Error' on failure.
    pub fn new(service_name: impl AsRef<str>) -> Result<InstanceSpecifier> {
        let service_name = service_name.as_ref();
        if Self::check_str(service_name) {
            Ok(Self {
                specifier: service_name.to_string(),
            })
        } else {
            Err(Error::Fail)
        }
    }
}

impl TryFrom<&str> for InstanceSpecifier {
    type Error = Error;
    fn try_from(s: &str) -> Result<Self> {
        Self::new(s)
    }
}

impl AsRef<str> for InstanceSpecifier {
    fn as_ref(&self) -> &str {
        &self.specifier
    }
}

/// Specifies whether to find a specific service instance or any available instance
pub enum FindServiceSpecifier {
    Specific(InstanceSpecifier),
    Any,
}

/// Convert an InstanceSpecifier into a FindServiceSpecifier
impl Into<FindServiceSpecifier> for InstanceSpecifier {
    fn into(self) -> FindServiceSpecifier {
        FindServiceSpecifier::Specific(self)
    }
}

/// This trait shall ensure that we can safely use an instance of the implementing type across
/// address boundaries. This property may be violated by the following circumstances:
/// - usage of pointers to other members of the struct itself (akin to !Unpin structs)
/// - usage of Rust pointers or references to other data
///
/// This can be trivially achieved by not using any sort of reference. In case a reference (either
/// to self or to other data) is required, the following options exist:
/// - Use indices into other data members of the same structure
/// - Use offset pointers _to the same memory chunk_ that point to different (external) data
///
/// # Safety
///
/// Since it is yet to be proven whether this trait can be implemented safely (assumption is: no) it
/// is unsafe for now. The expectation is that very few users ever need to implement this manually.

/// A `Sample` provides a reference to a memory buffer of an event with immutable value.
///
/// By implementing the `Deref` trait implementations of the trait support the `.` operator for dereferencing.
/// The buffers with its data lives as long as there are references to it existing in the framework.
///
/// The ordering of SamplePtrs is total over the reception order
/// # Type Parameters
/// * `T` - The relocatable event data type
// TODO: C++ doesn't yet support this. Expose API to compare SamplePtr ages.
pub trait Sample<T>: Deref<Target = T> + Send + PartialOrd + Ord + Debug
where
    T: Send + Reloc + Debug,
{
}

/// A `SampleMut` provides a reference to a memory buffer of an event with mutable value.
///
/// By implementing the `DerefMut` trait implementations of the trait support the `.` operator for dereferencing.
/// The buffers with its data lives as long as there are references to it existing in the framework.
///
/// # Type Parameters
/// * `T` - The relocatable event data type
pub trait SampleMut<T>: DerefMut<Target = T> + Debug
where
    T: Send + Reloc + Debug,
{
    /// Sample type for immutable access
    type Sample: Sample<T>;

    /// Consume the sample into an immutable sample.
    ///
    /// # Returns
    ///
    /// A `Sample` instance providing immutable access to the data.
    fn into_sample(self) -> Self::Sample;

    /// Send the sample and consume it.
    ///
    /// # Returns
    ///
    /// A `Result` indicating success or failure of the send operation.
    fn send(self) -> Result<()>;
}

/// A `SampleMaybeUninit` provides a reference to a memory buffer of an event with a `MaybeUninit` value.
///
/// The buffer can be assumed initialized with mutable access by calling `assume_init` which returns a `SampleMut`.
/// The buffers with its data lives as long as there are references to it existing in the framework.
///
/// # Type Parameters
/// * `T` - The relocatable event data type
///
/// TODO: Shall we also require DerefMut<Target=MaybeUninit<T>> from implementing types? How to deal
/// TODO: with the ambiguous assume_init() then?
pub trait SampleMaybeUninit<T>: Debug + AsMut<core::mem::MaybeUninit<T>>
where
    T: Send + Reloc + Debug,
{
    /// Buffer type for mutable data after initialization
    type SampleMut: SampleMut<T>;
    /// Render the buffer initialized for mutable access.
    ///
    /// This corresponds to `MaybeUninit::assume_init`.
    ///
    /// TODO: Collision with MaybeUninit::assume_init() needs to be resolved.
    ///
    /// # Safety
    ///
    /// The caller has to make sure to initialize the data in the buffer before calling this method.
    ///
    /// # Returns
    ///
    /// A `SampleMut` instance providing mutable access to the initialized data.
    unsafe fn assume_init(self) -> Self::SampleMut;
    /// Write a value into the buffer and render it initialized.
    ///
    /// This corresponds to `MaybeUninit::write`.
    ///
    /// # Returns
    ///
    /// A `SampleMut` instance providing mutable access to the initialized data.
    fn write(self, value: T) -> Self::SampleMut;
}

/// Service interface contract definition.
///
/// Defines the communication schema and roles for a particular service interface.
/// Implementations specify the consumer and producer types that implement the interface contract.
pub trait Interface {
    /// Unique type identifier for this interface
    const TYPE_ID: &'static str;
    /// consumer type for this interface
    type Consumer<R: Runtime + ?Sized>: Consumer<R>;
    /// producer type for this interface
    type Producer<R: Runtime + ?Sized>: Producer<R>;
}

/// Service instance currently offered to the system.
///
/// Represents an actively advertised service that can be discovered and consumed by clients.
/// The offered producer can be withdrawn from the system via the `unoffer` operation.
///
/// # Type Parameters
/// * `R` - The runtime implementation managing this offered service
#[must_use = "if a service is offered it will be unoffered and dropped immediately, causing unexpected behavior in the system"]
pub trait OfferedProducer<R: Runtime + ?Sized> {
    /// Interface type of the producer
    type Interface: Interface;
    /// Producer type of the offered producer
    type Producer: Producer<R, Interface = Self::Interface>;

    /// Withdraw the service from system availability.
    ///
    /// # Returns
    ///
    /// The original producer instance after withdrawal.
    fn unoffer(self) -> Self::Producer;
}

/// Service instance to be offered to the system.
///
/// Represents an implementer of a service interface that can be advertised to make it
/// discoverable and accessible by consumer applications.
///
/// # Type Parameters
/// * `R` - The runtime implementation managing this service
pub trait Producer<R: Runtime + ?Sized> {
    /// Interface type of the producer
    type Interface: Interface;
    /// Offered producer type after offering the service
    type OfferedProducer: OfferedProducer<R, Interface = Self::Interface>;

    /// Register the service instance with the runtime for discovery.
    ///
    /// # Returns
    ///
    /// A 'Result' containing the offered producer on success and an 'Error' on failure.
    fn offer(self) -> Result<Self::OfferedProducer>;

    /// Create a new producer instance from the provided instance information.
    ///
    /// # Parameters
    /// * `instance_info` - Runtime-specific configuration for this producer instance
    /// # Returns
    /// A 'Result' containing the constructed producer on success and an 'Error' on failure.
    fn new(instance_info: R::ProviderInfo) -> Result<Self>
    where
        Self: Sized;
}

/// Event publication interface for streaming data to subscribers.
/// Manages the allocation and transmission of event samples
/// # Type Parameters
/// * `T` - The relocatable event data type
pub trait Publisher<T, R: Runtime + ?Sized>
where
    T: Reloc + Send + Debug,
{
    /// Associated sample type for uninitialized event data
    type SampleMaybeUninit<'a>: SampleMaybeUninit<T> + 'a
    where
        Self: 'a;
    /// Allocate a buffer slot for the event publication.
    ///
    /// # Returns
    ///
    /// A 'Result' containing the allocated sample buffer on success and an 'Error' on failure.
    fn allocate<'a>(&'a self) -> Result<Self::SampleMaybeUninit<'a>>;

    /// Allocate, initialize, and send an event sample in one step.
    ///
    /// # Parameters
    /// * `value` - The event data to publish
    ///
    /// # Returns
    ///
    /// A 'Result' indicating success or failure of the send operation.
    fn send(&self, value: T) -> Result<()> {
        let sample = self.allocate()?;
        let init_sample = sample.write(value);
        init_sample.send()
    }

    /// Create a new publisher for the specified event source.
    ///
    /// # Parameters
    /// * `identifier` - Logical name of the event topic
    /// * `instance_info` - Runtime-specific configuration for the event source
    /// # Returns
    /// A 'Result' containing the constructed publisher on success and an 'Error' on failure.
    fn new(identifier: &str, instance_info: R::ProviderInfo) -> Result<Self>
    where
        Self: Sized;
}

/// Consumer role implementation for a specific service interface.
///
/// # Type Parameters
/// * `R` - The runtime implementation managing this consumer
pub trait Consumer<R: Runtime + ?Sized> {
    /// Create a new consumer instance from the provided instance information.
    ///
    /// # Parameters
    /// * `instance_info` - Runtime-specific configuration for this consumer instance
    ///
    /// # Returns
    ///
    /// A new consumer instance configured with the provided instance information.
    fn new(instance_info: R::ConsumerInfo) -> Self;
}

/// Builder for creating configured producer instances.
///
/// Extends the base builder pattern with producer-specific configuration steps
/// before instantiation.
///
/// # Type Parameters
/// * `I` - The service interface being offered
/// * `P` - The producer type being constructed
/// * `R` - The runtime managing the producer
pub trait ProducerBuilder<I: Interface, P: Producer<R, Interface = I>, R: Runtime + ?Sized>:
    Builder<P>
{
}

/// Service registry and discovery interface.
///
/// Locates available instances of a service interface within the system.
///
/// # Type Parameters
/// * `I` - The service interface to discover
/// * `R` - The runtime managing service discovery
pub trait ServiceDiscovery<I: Interface, R: Runtime + ?Sized> {
    /// Builder type for constructing consumers from discovered services
    type ConsumerBuilder: ConsumerBuilder<I, R>;
    /// ServiceEnumerator type for iterating over available service instances
    type ServiceEnumerator: IntoIterator<Item = Self::ConsumerBuilder>;

    /// Query available instances of this service interface.
    ///
    /// # Returns
    ///
    /// An iterator over builders for each discovered instance. Returns empty
    /// results if no instances are currently available.
    fn get_available_instances(&self) -> Result<Self::ServiceEnumerator>;

    /// Asynchronous version of querying available instances of this service interface.
    ///
    /// # Returns
    ///
    #[allow(clippy::manual_async_fn)]
    fn get_available_instances_async(
        &self,
    ) -> impl Future<Output = Result<Self::ServiceEnumerator>> + Send;
}

/// Metadata and identification for a discovered service instance.
///
/// Provides runtime-specific identifiers and properties for a service instance
/// discovered during service discovery operations.
///
/// # Type Parameters
/// * `R` - The runtime managing this service
pub trait ConsumerDescriptor<R: Runtime + ?Sized> {
    /// Get the unique instance specifier for this service instance.
    fn get_instance_identifier(&self) -> &InstanceSpecifier;
}

/// Constructor for consumer instances of a specific service.
///
/// Combines service metadata with the ability to construct fully-configured
/// consumer endpoints through the builder pattern.
///
/// # Type Parameters
/// * `I` - The service interface
/// * `R` - The runtime managing the consumer
pub trait ConsumerBuilder<I: Interface, R: Runtime + ?Sized>:
    ConsumerDescriptor<R> + Builder<I::Consumer<R>>
{
}

/// Event subscription management interface.
///
/// Establishes a subscription channel to receive publications from a specific event source.
/// Subscriptions support both polling and async-await patterns for consuming events.
///
/// # Type Parameters
/// * `T` - The relocatable event data type
/// * `R` - The runtime managing the subscription
pub trait Subscriber<T: Reloc + Send + Debug, R: Runtime + ?Sized> {
    /// Associated subscription type for receiving event samples
    type Subscription: Subscription<T, R>;

    /// Create a subscriber for the specified event source.
    ///
    /// # Parameters
    /// * `identifier` - Logical name of the event topic
    /// * `instance_info` - Runtime-specific configuration for the event source
    fn new(identifier: &str, instance_info: R::ConsumerInfo) -> Result<Self>
    where
        Self: Sized;

    /// Establish a subscription to the event source.
    ///
    ///  Parameters
    /// * `max_num_samples` - Maximum number of samples to buffer for this subscription
    ///
    /// # Returns
    /// A subscription handle for receiving event samples.
    /// Fails if the subscription cannot be established due to resource constraints
    /// or if the event source is not available.
    fn subscribe(&self, max_num_samples: usize) -> Result<Self::Subscription>;
}
/// A container for samples received from a subscription.
/// Provides methods to manipulate and access the samples.
///
/// # Type Parameters
/// * `S`: The sample type stored in the container.
pub struct SampleContainer<S> {
    inner: VecDeque<S>,
}

impl<S> Default for SampleContainer<S> {
    fn default() -> Self {
        Self {
            inner: VecDeque::new(),
        }
    }
}

impl<S> SampleContainer<S> {
    /// Creates a new, empty `SampleContainer`
    pub fn new() -> Self {
        Self::default()
    }

    /// Returns an iterator over references to the samples in the container.
    ///
    /// # Type Parameters
    /// * `T`: The type of the samples to iterate over.
    ///
    /// # Returns
    /// An iterator over references to the samples of type `T`.
    pub fn iter<'a, T>(&'a self) -> impl Iterator<Item = &'a T>
    where
        S: Sample<T>,
        T: Reloc + Send + 'a + Debug,
    {
        self.inner.iter().map(<S as Deref>::deref)
    }

    /// Removes and returns the first sample from the container, if any.
    ///
    /// # Returns
    /// An `Option` containing the removed sample, or `None` if the container is empty.
    pub fn pop_front(&mut self) -> Option<S> {
        self.inner.pop_front()
    }

    /// Adds a new sample to the back of the container.
    ///
    /// # Parameters
    /// * `new` - The new sample to add to the container.
    ///
    /// # Returns
    /// A `Result` indicating success or failure.
    pub fn push_back(&mut self, new: S) -> Result<()> {
        self.inner.push_back(new);
        Ok(())
    }

    /// Returns the number of samples in the container.
    ///
    /// # Returns
    /// The number of samples currently stored in the container.
    pub fn sample_count(&self) -> usize {
        self.inner.len()
    }

    /// Returns a reference to the first sample in the container, if any.
    ///
    /// # Returns
    /// An `Option` containing a reference to the first sample, or `None` if the container is empty.
    pub fn front<T: Reloc + Send + Debug>(&self) -> Option<&T>
    where
        S: Sample<T>,
    {
        self.inner.front().map(<S as Deref>::deref)
    }
}

/// Active event subscription with polling and async receive capabilities.
///
/// Represents a live subscription to an event source with methods for both
/// non-blocking polling and asynchronous waiting for new events.
///
/// # Type Parameters
/// * `T` - The relocatable event data type
/// * `R` - The runtime managing the subscription
pub trait Subscription<T: Reloc + Send + Debug, R: Runtime + ?Sized> {
    /// Associated subscriber type for managing the subscription lifecycle
    type Subscriber: Subscriber<T, R>;
    /// Associated sample type for received event data
    type Sample<'a>: Sample<T>
    where
        Self: 'a;

    /// Unsubscribe from the event source.
    ///
    /// Consumes the subscription and returns the original subscriber.
    ///
    /// Returns
    /// The subscriber used to create this subscription.
    fn unsubscribe(self) -> Self::Subscriber;

    /// Returns up to max_samples samples.
    ///
    /// This call polls for up to max_new samples from the IPC input buffers without blocking the
    /// calling thread.
    ///
    /// The method expects a (possibly empty) buffer that may contain samples from a previous call
    /// to `try_receive` of the same `Subscription` instance. If there are samples from other
    /// instances, the method fails. These sample pointers can then be reused by the method:
    ///
    /// TODO: How to make sure that the provided container contains samples from the same
    /// TODO: `Subscription` instance?
    ///
    /// - If there are less than max_samples in the communication buffer, the method will reuse
    ///   samples contained in the provided sample container, beginning from the last, going
    ///   backwards.
    /// - If the input container is empty on call, it will exclusively be filled with new samples.
    /// - If the input container contains more samples than max_samples before the call,
    ///   it will contain max_samples samples, potentially removing samples from
    ///   the container, even if no new samples were added.
    ///
    /// Returns the updated buffer and the number of newly added samples. All new samples are added
    /// to the back of the buffer, with the last sample being the newest. If less than max_samples
    /// could be added to the buffer, the samples that had been inside the buffer are retained, with
    /// the first samples getting removed as new samples are added to the back of the buffer.
    ///
    /// TODO: C++ cannot fully support this yet since there is no way to retain potentially-reusable
    /// TODO: samples.
    /// # Parameters
    /// * `scratch` - Container for events from this subscription; must not be reused across
    ///   different subscriptions
    /// * `max_samples` - Maximum number of events to transfer
    ///
    /// # Returns
    ///
    /// Number of newly added events to the container
    fn try_receive<'a>(
        &'a self,
        scratch: &'_ mut SampleContainer<Self::Sample<'a>>,
        max_samples: usize,
    ) -> Result<usize>;

    /// This method returns a future that resolves as soon as at least `new_samples` samples have
    /// been transferred from the communication buffer to the sample container.
    ///
    /// The replacement semantics as well as the post conditions of the resolved future are equal
    /// to `try_receive`.
    ///
    /// TODO: See above for C++ limitations.
    /// # Parameters
    /// * `scratch` - Container for events from this subscription
    /// * `new_samples` - Minimum number of new events before resolution
    /// * `max_samples` - Maximum total capacity of the container
    ///
    /// # Returns
    ///
    /// Number of newly added events to the container
    fn receive<'a>(
        &'a self,
        scratch: &'_ mut SampleContainer<Self::Sample<'a>>,
        new_samples: usize,
        max_samples: usize,
    ) -> impl Future<Output = Result<usize>> + Send;
}

///Test module for InstanceSpecifier validation
mod tests {
    #[test]
    fn test_instance_specifier_validation() {
        use super::InstanceSpecifier;
        // Valid specifiers
        let valid_specifiers = [
            "/my/service",
            "/my/path/to/service_name",
            "/Service_123/AnotherPart",
            "/A",
            "/A/abc_123/Xyz",
        ];

        for spec in &valid_specifiers {
            assert!(
                InstanceSpecifier::check_str(spec),
                "Expected '{}' to be valid",
                spec
            );
        }

        // Invalid specifiers
        let invalid_specifiers = [
            "my/service",           // No leading slash
            "/my//service",         // Consecutive slashes
            "/my/service/",         // Trailing slash
            "/my/ser!vice",         // Illegal character '!'
            "/my/ser vice",         // Illegal character ' '
            "/",                    // Only root slash
            "/my/path//to/service", // Consecutive slashes in the middle
            "/my/path/to//",        // Trailing consecutive slashes
            "//my/service",         // Leading consecutive slashes
            "///my/service",        // Leading consecutive slashes
        ];

        for spec in &invalid_specifiers {
            assert!(
                !InstanceSpecifier::check_str(spec),
                "Expected '{}' to be invalid",
                spec
            );
        }
    }
}
