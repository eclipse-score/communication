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
pub use com_api_concept_macros::CommData;
use containers::fixed_capacity::FixedCapacityQueue;
pub use reloc::Reloc;
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

/// Result type alias with `std::result::Result` using `com_api::Error` as error type
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
    ///
    ///  # Errors
    ///
    ///  Errors that may occur during the build process captured in the 'Result'.
    /// TODO: Should this be &mut self so that this can be turned into a trait object?
    fn build(self) -> Result<Output>;
}

/// This represents the com implementation and acts as a root for all types and objects provided by
/// the implementation.
pub trait Runtime {
    /// `ServiceDiscovery<I>` types for Discovers available service instances of a specific interface
    type ServiceDiscovery<I: Interface>: ServiceDiscovery<I, Self>;

    /// `Subscriber<T>` types for Manages subscriptions to event notifications
    type Subscriber<T: CommData>: Subscriber<T, Self>;

    /// `ProducerBuilder<I>` types for Constructs producer instances for offering services
    type ProducerBuilder<I: Interface>: ProducerBuilder<I, Self>;

    /// `Publisher<T>` types for Publishes event data to subscribers
    type Publisher<T: CommData>: Publisher<T, Self>;

    /// `ProviderInfo` types for Configuration data for service producers instances
    type ProviderInfo: ProviderInfo + Send + Clone;

    /// `ConsumerInfo` types for Configuration data for service consumers instances
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
    fn producer_builder<I: Interface>(
        &self,
        instance_specifier: InstanceSpecifier,
    ) -> Self::ProducerBuilder<I>;
}

/// This trait contains the APIs required for producer service instance management.
/// But this APIs are not user facing APIs, it is used internally by the runtime implementations.
/// This trait is implemented for the ProviderInfo types of the runtimes.
pub trait ProviderInfo {
    /// Offer a service instance to make it discoverable and accessible by consumers.
    /// Registers the service instance with the runtime for discovery.
    /// This API must make service instance available to consumers.
    ///
    /// # Parameters
    /// * `self` - The provider information containing configuration for the service instance
    ///
    /// # Returns
    /// A 'Result' indicating success or failure of the offer operation.
    fn offer_service(&self) -> Result<()>;

    /// Stop offering a service instance, making it no longer discoverable.
    /// Withdraw the service from system availability.
    ///
    /// # Parameters
    /// * `self` - The provider information containing configuration for the service instance
    ///
    /// # Returns
    /// A 'Result' indicating success or failure of the unoffer operation.
    fn stop_offer_service(&self) -> Result<()>;
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

/// Communication data type requirements trait.
/// Bounds the data type to be relocatable and sendable across threads.
/// Also requires the data type to implement Debug for logging and debugging purposes.
/// 'static' lifetime ensures the data type does not contain non-static references.
/// # Important
/// Users must NOT implement the `CommData` trait manually. Always use this derive macro instead.
/// like `#[derive(CommData)]` on the struct definition. 
/// This is to ensure that all necessary trait bounds and metadata are correctly applied to the communication data types.
/// Also if user wants to specify a custom ID for the communication data type, 
/// they can use the `#[comm_data(id = "CustomID")]` attribute on the struct definition. 
/// The custom ID must be unique across all communication data types to avoid conflicts in the system.
/// If no custom ID is provided, the struct name will be used as the default ID with module path as prefix to ensure uniqueness.
pub trait CommData: Reloc + Send + Debug + 'static {
    const ID: &'static str;
}

/// Technology independent description of a service instance "location"
///
/// The string shall describe where to find a certain instance of a service. Each level shall look
/// like this
///  `/my/path/to/service_name`
/// validation for service name- `/my/path/to/service_name`
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
        // Allowed: digits, lowercase, uppercase, underscore
        let is_legal_char = |c: char| {
            c.is_ascii_digit() || c.is_ascii_lowercase() || c.is_ascii_uppercase() || c == '_'
        };

        //validation of each path segment
        !service_name.is_empty()
            && service_name.split('/').all(|parts| {
                // No empty segments (reject trailing "/" and "//" in the middle)
                !parts.is_empty() && parts.chars().all(&is_legal_char)
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
    /// A `Result` containing the constructed `InstanceSpecifier` on success and an `Error` on failure.
    ///
    /// # Errors
    /// Returns `Error::Fail` if the provided string does not conform to the required format.
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

/// Convert an `InstanceSpecifier` From a `FindServiceSpecifier`
impl From<InstanceSpecifier> for FindServiceSpecifier {
    fn from(specifier: InstanceSpecifier) -> FindServiceSpecifier {
        FindServiceSpecifier::Specific(specifier)
    }
}

/// A `Sample` provides a reference to a memory buffer of an event with immutable value.
///
/// By implementing the `Deref` trait implementations of the trait support the `.` operator for dereferencing.
/// The buffers with its data lives as long as there are references to it existing in the framework.
///
/// The ordering of `SamplePtrs` is total over the reception order
/// # Type Parameters
/// * `T` - The relocatable event data type
// TODO: C++ doesn't yet support this. Expose API to compare SamplePtr ages.
pub trait Sample<T>: Deref<Target = T> + Send + PartialOrd + Ord + Debug
where
    T: CommData,
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
    T: CommData,
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
    ///
    /// # Errors
    /// Returns 'Error' if the send operation fails.
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
/// TODO: Shall we also require `DerefMut`<Target=MaybeUninit<T>> from implementing types? How to deal
/// TODO: with the ambiguous `assume_init()` then?
pub trait SampleMaybeUninit<T>: Debug + AsMut<core::mem::MaybeUninit<T>>
where
    T: CommData,
{
    /// Buffer type for mutable data after initialization
    type SampleMut: SampleMut<T>;
    /// Render the buffer initialized for mutable access.
    ///
    /// This corresponds to `MaybeUninit::assume_init`.
    ///
    /// TODO: Collision with `MaybeUninit::assume_init()` needs to be resolved.
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

    /// Writes in place (skipping intermediate moves) the default value directly into the buffer and renders it initialized.
    /// This requires that T implements `PlacementDefault`.
    fn write_default(mut self) -> Self::SampleMut
    where
        Self: Sized,
        T: PlacementDefault,
    {
        T::placement_default(self.as_mut().as_mut_ptr());

        // Safety: `placement_default` initialized the data
        unsafe { self.assume_init() }
    }
}

/// Service interface contract definition.
///
/// Defines the communication schema and roles for a particular service interface.
/// Implementations specify the consumer and producer types that implement the interface contract.
pub trait Interface {
    /// Unique type identifier for this interface
    const INTERFACE_ID: &'static str;
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
    fn unoffer(self) -> Result<Self::Producer>;
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
    ///
    /// # Errors
    /// Returns 'Error' if the offer operation fails.
    fn offer(self) -> Result<Self::OfferedProducer>;

    /// Create a new producer instance from the provided instance information.
    ///
    /// # Parameters
    /// * `instance_info` - Runtime-specific configuration for this producer instance
    /// # Returns
    /// A 'Result' containing the constructed producer on success and an 'Error' on failure.
    /// # Errors
    /// Returns 'Error' if the producer cannot be created with the provider information.
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
    T: CommData,
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
    ///
    /// # Errors
    ///
    /// Returns 'Error' if the allocation fails.
    //Since &self is the only parameter with a lifetime,
    // Rust's Lifetime Elision Rules automatically bind the return type's lifetime to the implicit lifetime of &self,
    // This is Elision Rule: When there's exactly one input lifetime,
    // it's automatically used for all elided output lifetimes The compiler implicitly expands this to:
    // fn allocate(&'a self) -> Result<Self::SampleMaybeUninit<'a>>
    fn allocate(&self) -> Result<Self::SampleMaybeUninit<'_>>;

    /// Allocate, initialize, and send an event sample in one step.
    ///
    /// # Parameters
    /// * `value` - The event data to publish
    ///
    /// # Returns
    ///
    /// A 'Result' indicating success or failure of the send operation.
    ///
    /// # Errors
    ///
    /// Returns 'Error' if the send operation fails.
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
    /// # Errors
    /// Returns 'Error' if the publisher cannot be created with the provider information.
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
/// * `R` - The runtime managing the producer
pub trait ProducerBuilder<I: Interface, R: Runtime + ?Sized>: Builder<I::Producer<R>> {}

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
    /// `ServiceEnumerator` type for iterating over available service instances
    type ServiceEnumerator: IntoIterator<Item = Self::ConsumerBuilder>;

    /// Query available instances of this service interface.
    ///
    /// # Returns
    ///
    /// An iterator over builders for each discovered instance. Returns empty
    /// results if no instances are currently available.
    ///
    /// # Errors
    /// Returns 'Error' if the query operation fails.
    fn get_available_instances(&self) -> Result<Self::ServiceEnumerator>;

    /// Asynchronous version of querying available instances of this service interface.
    ///
    /// # Returns
    /// Future that resolves to an iterator over builders for each discovered instance.
    /// Returns empty results if no instances are currently available.
    ///
    /// # Errors
    /// Returns 'Error' if the query operation fails.
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
pub trait Subscriber<T: CommData, R: Runtime + ?Sized> {
    /// Associated subscription type for receiving event samples
    type Subscription: Subscription<T, R>;

    /// Create a subscriber for the specified event source.
    ///
    /// # Parameters
    /// * `identifier` - Logical name of the event topic
    /// * `instance_info` - Runtime-specific configuration for the event source
    ///   Creates a new `Consumer` instance.
    ///
    /// # Errors
    ///
    /// Returns an error if the consumer cannot be created with the given identifier and instance info.
    fn new(identifier: &'static str, instance_info: R::ConsumerInfo) -> Result<Self>
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
    ///
    /// # Errors
    /// Returns 'Error' if the subscription cannot be established.
    fn subscribe(&self, max_num_samples: usize) -> Result<Self::Subscription>;
}
/// A container for samples received from a subscription.
/// Provides methods to manipulate and access the samples.
///
/// # Type Parameters
/// * `S`: The sample type stored in the container.
pub struct SampleContainer<S> {
    inner: FixedCapacityQueue<S>,
}

impl<S> SampleContainer<S> {
    /// Creates a new, empty `SampleContainer`
    pub fn new(capacity: usize) -> Self {
        Self {
            inner: FixedCapacityQueue::new(capacity),
        }
    }

    /// Returns an iterator over references to the samples in the container.
    ///
    /// # Type Parameters
    /// * `T`: The type of the samples to iterate over.
    ///
    /// # Returns
    /// An iterator over references to the samples of type `T`.
    pub fn iter<T>(&self) -> impl Iterator<Item = &T>
    where
        S: Sample<T>,
        T: CommData,
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
    ///
    /// # Errors
    /// Returns 'Error' if container is already full.
    pub fn push_back(&mut self, new: S) -> Result<()> {
        self.inner
            .push_back(new)
            .map_err(|_| Error::AllocateFailed)?;
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
    pub fn front<T: CommData>(&self) -> Option<&T>
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
pub trait Subscription<T: CommData, R: Runtime + ?Sized> {
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

    /// Returns up to `max_samples` samples.
    ///
    /// This call polls for up to `max_new` samples from the IPC input buffers without blocking the
    /// calling thread.
    ///
    /// The method expects a (possibly empty) buffer that may contain samples from a previous call
    /// to `try_receive` of the same `Subscription` instance. If there are samples from other
    /// instances, the method fails. These sample pointers can then be reused by the method:
    ///
    /// TODO: How to make sure that the provided container contains samples from the same
    /// TODO: `Subscription` instance?
    ///
    /// - If there are less than `max_samples` in the communication buffer, the method will reuse
    ///   samples contained in the provided sample container, beginning from the last, going
    ///   backwards.
    /// - If the input container is empty on call, it will exclusively be filled with new samples.
    /// - If the input container contains more samples than `max_samples` before the call,
    ///   it will contain `max_samples` samples, potentially removing samples from
    ///   the container, even if no new samples were added.
    ///
    /// Returns the updated buffer and the number of newly added samples. All new samples are added
    /// to the back of the buffer, with the last sample being the newest. If less than `max_samples`
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
    ///
    /// # Errors
    /// Returns 'Error' if the receive operation fails.
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

/// A trait for types that can be default-constructed in place, skipping intermediate moves.
///
/// # Safety
/// The implementer must ensure that the `placement_default` method correctly initializes the
/// memory at the given pointer to a valid instance of the type, without creating temporary
/// references that would lead to undefined behavior. The `value` under pointer after call to
/// `placement_default` is considered initialized.
pub unsafe trait PlacementDefault: Default {
    /// Writes in place (skipping intermediate moves) the default value directly into the buffer.
    ///
    /// # Safety
    ///  - `ptr` shall be assumed the pointer to uninitialized memory
    ///  - implementer shall use `&raw FIELD` access to write data into fields and not producing
    ///    temporary references that would cause undefined behavior
    fn placement_default(ptr: *mut Self);
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
