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

//! The **COM API** is a Rust API for service-oriented inter-process communication (IPC) in the `mw::com` ecosystem.
//!
//! Build service **providers** and **consumers** using a publish/subscribe model.
//! Application business logic is written against the runtime-agnostic traits re-exported here, the concrete
//! runtime (e.g. [`LolaRuntimeBuilderImpl`] for production, [`MockRuntimeBuilderImpl`] for tests)
//! is selected at initialisation time without changing application code.
//!
//! # Design Principles
//! The COM API follows these core design principles:
//! - **Builder Pattern**: All complex objects use the builder pattern for type-safe construction
//! - **Zero-Copy Communication**: Supports efficient data transmission using pre-allocated, position-independent memory
//! - **No Heap Allocation During Runtime**: Heap memory is allowed only during initialization phase, runtime operations use pre-allocated chunks
//! - **Type Safety**: Derives type information from interface descriptions to prevent misuse
//! - **Runtime Agnostic**: The API does not enforce the necessity for internal threads or executors
//!
//! # Core concepts
//!
//! | Type | Role |
//! |------|------|
//! | [`RuntimeBuilder`] / [`Runtime`] | Entry point; creates producers and drives service discovery. |
//! | [`Producer`] / [`OfferedProducer`] | Provider side: offers a service instance and holds [`Publisher`]s. |
//! | [`Consumer`] | User side: connects to a discovered instance and holds [`Subscriber`]s. |
//! | [`ServiceDiscovery`] | Finds available service instances; availability can change over time. |
//! | [`Publisher<T>`] / [`Subscriber<T>`] | Typed send / receive endpoints for one event. |
//! | [`Subscription<T>`] | Represents an active event stream. |
//! | [`Sample<T>`] | `Sample<T>` values are immutable snapshots you read from it. |
//! | [`SampleContainer`] | Container for reading samples from the event stream. |
//! | [`InstanceSpecifier`] | Path-like service address (e.g. `/vehicle/speed`). |
//!
//! # Important constraints
//! - **Fixed capacity**: after initialization, subscription queues and sample containers are fixed-size.
//!   During initialization, capacity is fully dynamic.
//! - **Explicit lifetimes**: dropping an [`OfferedProducer`] withdraws the service, samples must
//!   not outlive their source.
//! - **Sample lifetimes**: samples are valid only until the subscription instance is valid.
//! - **LoLa note**: [`FindServiceSpecifier::Any`] is not yet supported — use `Specific` only.
//!
//! # Quick Start
//!
//! ## Producer (Service Provider)
//! ```ignore
//! use com_api::*;
//! use std::path::Path;
//!
//! // 1. Initialize runtime with configuration
//! let mut builder = LolaRuntimeBuilderImpl::new();
//! builder.load_config(Path::new("etc/mw_com_config.json"));
//! let runtime = builder.build()?;
//!
//! // 2. Create producer with builder pattern
//! let spec = InstanceSpecifier::new("/vehicle/speed")?;
//! let producer = runtime.producer_builder::<VehicleInterface>(spec).build()?;
//!
//! // 3. Offer service to make it discoverable
//! let offered = producer.offer()?;
//!
//! // 4. Publish events using typed publisher and zero-copy samples
//! let sample = offered.left_tire.allocate()?;
//! let sample = sample.write(Tire { pressure: 33.0 });
//! sample.send()?;
//!
//! // 5. Withdraw service when done
//! let _producer = offered.unoffer()?;
//! ```
//!
//! ## Consumer (Service Client)
//! ```ignore
//! use com_api::*;
//! use std::path::Path;
//!
//! // 1. Initialize runtime
//! let mut builder = LolaRuntimeBuilderImpl::new();
//! builder.load_config(Path::new("etc/mw_com_config.json"));
//! let runtime = builder.build()?;
//!
//! // 2. Discover service instance
//! let spec = InstanceSpecifier::new("/vehicle/speed")?;
//! let discovery = runtime.find_service::<VehicleInterface>(
//!     FindServiceSpecifier::Specific(spec)
//! );
//! let instances = discovery.get_available_instances()?;
//!
//! // 3. Create consumer from discovered instance
//! let consumer = instances.into_iter()
//!     .next()
//!     .ok_or(Error::Fail)?
//!     .build()?;
//!
//! // 4. Subscribe to events with fixed-capacity buffer
//! let subscription = consumer.left_tire.subscribe(3)?;
//! let mut container = SampleContainer::new(3);
//!
//! // 5. Poll for events (non-blocking)
//! match subscription.try_receive(&mut container, 3) {
//!     Ok(n) if n > 0 => {
//!         while let Some(sample) = container.pop_front() {
//!             println!("Tire pressure: {}", sample.pressure);
//!         }
//!     }
//!     _ => { /* No samples available */ }
//! }
//!
//! // 6. Or wait asynchronously for events
//! let n = subscription.receive(&mut container, 1, 3).await?;
//! ```
//! # Further reading
//! - `com_api_concept` crate — trait definitions and full API documentation
//! - `doc/com_api_high_level_design_detail.md` — internal architecture and layer details

pub use com_api_runtime_lola::LolaRuntimeImpl;
pub use com_api_runtime_lola::RuntimeBuilderImpl as LolaRuntimeBuilderImpl;
pub use com_api_runtime_mock::MockRuntimeImpl;
pub use com_api_runtime_mock::RuntimeBuilderImpl as MockRuntimeBuilderImpl;

pub use com_api_concept::{
    interface, interface_common, interface_consumer, interface_producer, Builder, CommData,
    Consumer, ConsumerBuilder, ConsumerDescriptor, Error, FindServiceSpecifier, InstanceSpecifier,
    Interface, OfferedProducer, PlacementDefault, Producer, ProducerBuilder, ProviderInfo,
    Publisher, Reloc, Result, Runtime, RuntimeBuilder, SampleContainer, SampleMaybeUninit,
    SampleMut, ServiceDiscovery, Subscriber, Subscription,
};

#[doc(hidden)]
// See eclipse-score/communication/issues/173 - `paste`crate is still in discussion regarding rust safety certification.
pub use com_api_concept::paste;
