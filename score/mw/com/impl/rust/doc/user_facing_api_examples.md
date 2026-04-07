<!--
Copyright (c) 2026 Contributors to the Eclipse Foundation

See the NOTICE file(s) distributed with this work for additional
information regarding copyright ownership.

This program and the accompanying materials are made available under the
terms of the Apache License Version 2.0 which is available at
https://www.apache.org/licenses/LICENSE-2.0

SPDX-License-Identifier: Apache-2.0
-->

# Rust COM API - User Facing API Examples

This document contains comprehensive examples of the Rust COM API for building inter-process communication systems using the `mw::com` framework.

## Runtime Configuration

The runtime (e.g. the LoLa backend) is initialised through a builder. Call `load_config()` with a file path to supply an implementation-specific configuration file before calling `build()`.

**Key Points**:
- `RuntimeBuilder` uses a fluent builder pattern, methods return `&mut Self` for chaining
- `load_config()` reads an implementation-specific configuration file (e.g. JSON)
- `build()` returns `Result<Runtime>` and fails fast if configuration is invalid

### Example: Initializing Runtime with Configuration

<details>
<summary>Examples:</summary>

```rust
use com_api::{LolaRuntimeBuilderImpl, Result, Runtime, RuntimeBuilder};
use std::path::Path;

// LOLA Runtime initialization with configuration
fn initialize_lola_runtime() -> Result<impl Runtime> {
    let config_path = Path::new("etc/mw_com_config.json");

    // Create builder and load configuration
    let mut builder = LolaRuntimeBuilderImpl::new();
    builder.load_config(config_path);

    // Build the runtime instance
    let runtime = builder.build()?;
    Ok(runtime)
}
```
</details>

---

### InstanceSpecifier

`InstanceSpecifier` is a validated, technology-independent path that uniquely identifies a service instance (e.g. `/vehicle/speed`). It is the primary handle used when creating producers, discovering services, and building consumers.

**Key Points**:
- `InstanceSpecifier::new()` validates path format at construction time,no silent failures
- Implements `AsRef<str>` for zero-copy string access
- Implements `TryFrom<&str>` for ergonomic fallible conversion
- Invalid paths (empty strings, consecutive slashes, or trailing slashes) return an error.

### Example: Creating and Using InstanceSpecifier

<details>
<summary>Examples:</summary>

```rust
use com_api::InstanceSpecifier;

// Creating a valid instance specifier
fn setup_service_instance() -> Result<()> {
    // Create specifier from path string
    let spec = InstanceSpecifier::new("/my/service/path")?;

    // Convert to string reference if needed
    let path_str: &str = spec.as_ref();
    println!("Service path: {}", path_str);

    Ok(())
}

// Using TryFrom trait for conversion
fn create_from_string(path: &str) -> Result<InstanceSpecifier> {
    // Implements TryFrom<&str>
    InstanceSpecifier::try_from(path)
}

// Error handling for invalid paths
fn safe_create_specifier(path: &str) -> InstanceSpecifier {
    InstanceSpecifier::new(path)
        .expect("Invalid service path format")
}
```
</details>

---

## Service Discovery

Service discovery is performed by calling `Runtime::find_service()`, which returns a discovery handle. From that handle, call `get_available_instances()` for a synchronous snapshot of currently available instances, or `get_available_instances_async()` to await the first matching instance asynchronously. Each entry in the returned iterator is a builder that constructs a `Consumer`.

**Key Points**:
- `FindServiceSpecifier::Specific` finds service matching the specified InstanceSpecifier
- `FindServiceSpecifier::Any` finds any available service (Note: LoLa does not support ANY InstanceSpecifier)
- Use `.nth(instance_index)` to select a specific instance from the enumerated services matching the InstanceSpecifier
- `.into_iter().next()` gets the first available instance matching the InstanceSpecifier (equivalent to `.nth(0)`)
- `get_available_instances()` returns immediately with current state as an iterator
- `get_available_instances_async()` provides async alternative for waiting

### Example: Finding and Discovering Services

<details>
<summary>Examples:</summary>

```rust
use com_api::{Runtime, InstanceSpecifier, FindServiceSpecifier};
use com_api_gen::VehicleInterface;

// Create consumer for the specified service identifier
fn create_consumer<R: Runtime>(
    runtime: &R,
    service_id: InstanceSpecifier,
) -> VehicleConsumer<R> {
    let consumer_discovery =
        runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(service_id));
    let available_service_instances = consumer_discovery.get_available_instances().unwrap();

    // Select service instance at specific index based on the InstanceSpecifier
    // The index corresponds to a particular instance of the service identified by service_id
    let instance_index = 0; // Index of the specific instance for this InstanceSpecifier (0 for first, 1 for second, etc.)
    let consumer_builder = available_service_instances
        .into_iter()
        .nth(instance_index)
        .unwrap();

    consumer_builder.build().unwrap()
}

// Polling-based service discovery with retry and timeout
fn find_service_with_retry<R: Runtime>(
    runtime: &R,
    service_id: InstanceSpecifier,
) -> Result<VehicleConsumer<R>> {
    let discovery = runtime.find_service::<VehicleInterface>(
        FindServiceSpecifier::Specific(service_id.clone())
    );

    const MAX_RETRIES: usize = 100;  // ~1 second timeout with 10ms sleep
    let mut retry_count = 0;

    // Poll until service is found or max retries exceeded
    loop {
        match discovery.get_available_instances() {
            Ok(services) => {
                // Try to get first available instance (index 0)
                if let Some(consumer_builder) = services.into_iter().next() {
                    println!("Found service instance");
                    return consumer_builder.build();
                }
                // Service not available yet, retry
                retry_count += 1;
                if retry_count >= MAX_RETRIES {
                    eprintln!("Service not found: discovery timeout exceeded");
                }
                std::thread::sleep(std::time::Duration::from_millis(10));
            }
            Err(e) => return Err(e),
        }
    }
}

// Asynchronous service discovery
async fn find_service_async<R: Runtime>(
    runtime: &R,
    service_id: InstanceSpecifier,
) -> Result<VehicleConsumer<R>> {
    let discovery = runtime.find_service::<VehicleInterface>(
        FindServiceSpecifier::Specific(service_id)
    );

    // Use async method to wait for service
    let services = discovery.get_available_instances_async().await?;

    // Select first available instance
    let consumer_builder = services
        .into_iter()
        .next()
        .expect("No service instance found");
    consumer_builder.build()
}
```
</details>

---

## Consumer API

A consumer is constructed from a successful service discovery result. Once a matching instance is found via `get_available_instances()`, call `build()` on the returned builder to create the consumer. The consumer provides typed `Subscriber` fields for each event in the interface, which can then be subscribed to independently.

**Key Points**:
- `create_consumer()` pattern directly builds consumer from discovered services based on InstanceSpecifier and instance index
- `subscribe()` establishes event stream on specific event publishers (e.g., `left_tire`)
- `try_receive()` polls for samples without blocking, returns number of new samples added
- `receive()` provides async alternative for waiting
- `SampleContainer` manages lifecycle and ordering of received samples
- `unsubscribe()` stops receiving events and returns the subscriber

### Example: Consuming Events from a Service

<details>
<summary>Examples:</summary>

```rust
use com_api::{Runtime, FindServiceSpecifier, InstanceSpecifier, SampleContainer};
use com_api_gen::{VehicleInterface, VehicleConsumer, Tire};

// Discover and create consumer for the service
fn create_consumer<R: Runtime>(
    runtime: &R,
    service_id: InstanceSpecifier,
) -> VehicleConsumer<R> {
    let consumer_discovery =
        runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(service_id));
    let available_service_instances = consumer_discovery.get_available_instances().unwrap();

    // Select service instance at specific index based on the InstanceSpecifier
    // The index corresponds to a particular instance of the service identified by service_id
    let instance_index = 0; // Index of the specific instance for this InstanceSpecifier (0 for first, 1 for second, etc.)
    let consumer_builder = available_service_instances
        .into_iter()
        .nth(instance_index)
        .unwrap();

    consumer_builder.build().unwrap()
}

// Polling for events (non-blocking)
fn read_tire_data<R: Runtime>(
    consumer: &VehicleConsumer<R>,
) -> Result<String> {
    let tire_subscriber = consumer.left_tire.subscribe(3).unwrap();
    let mut sample_buf = SampleContainer::new(3);

    match tire_subscriber.try_receive(&mut sample_buf, 1) {
        Ok(0) => println!("No samples available"),
        Ok(x) => {
            let sample = sample_buf.pop_front().unwrap();
            Ok(format!("{} samples received: sample[0] = {:?}", x, *sample))
        }
        Err(e) => Err(e),
    }
}
```
</details>

---

## Producer API

A producer is constructed by calling `Runtime::producer_builder()` with an `InstanceSpecifier`, then `build()` to create the producer, and finally `offer()` to make the service visible to consumers. While offered, the producer holds typed `Publisher` fields for each event defined in the interface.

**Key Points**:
- Producer instances must be offered before clients can connect
- Publishing data: `allocate() -> write() -> send()`
- Services remain available until `unoffer()` is called
- Dropped `OfferedProducer` automatically withdraws service

### Example: Implementing and Offering a Service

<details>
<summary>Examples:</summary>

```rust
use com_api::{Runtime, Producer, OfferedProducer, InstanceSpecifier};
use com_api_gen::{VehicleInterface, VehicleOfferedProducer, Tire};

// Create and offer a service producer
fn create_producer<R: Runtime>(
    runtime: &R,
    service_id: InstanceSpecifier,
) -> VehicleOfferedProducer<R> {
    let producer_builder = runtime.producer_builder::<VehicleInterface>(service_id);
    let producer = producer_builder.build().unwrap();
    producer.offer().unwrap()
}

// Publish tire events
fn write_tire_data<R: Runtime>(
    producer: &VehicleOfferedProducer<R>,
    tire: Tire,
) -> Result<()> {
    // Allocate buffer from shared memory
    let uninit_sample = producer.left_tire.allocate()?;
    // Initialize the sample using write method
    let sample = uninit_sample.write(tire);
    // Send the sample (moves ownership)
    sample.send()?;
    println!("Tire data sent");
    Ok(())
}

// Stopping service offering
fn stop_offering_service<R: Runtime>(
    producer: VehicleOfferedProducer<R>,
) -> Result<()> {
    // unoffer returns producer back
    match producer.unoffer() {
        Ok(_) => println!("Successfully unoffered the service"),
        Err(e) => eprintln!("Failed to unoffer: {:?}", e),
    }
    Ok(())
}
```
</details>

---

## Data Types

The COM API uses distinct types for the send and receive paths:

- `Sample<T>` — an immutable, reference-counted snapshot of received event data on the consumer side
- `SampleMut<T>` — a mutable handle to an allocated buffer on the producer side, consumed by `send()`
- `SampleMaybeUninit<T>` — an uninitialized buffer obtained via `allocate()` on the producer side, must be initialised with `write()` before sending.

All communicated types must implement `CommData`, which bounds `T: Reloc + Send + Debug + 'static`.

### Example: Working with Communication Data Types

<details>
<summary>Examples:</summary>

```rust
use com_api::{CommData, Sample, SampleMaybeUninit};
use com_api_gen::Tire;

// Tire data type defined in com_api_gen
#[derive(Debug, Clone, CommData)]
pub struct Tire {
    pub pressure: f32,
}

// Ordering samples by reception time
fn compare_samples(
    sample1: &impl Sample<Tire>,
    sample2: &impl Sample<Tire>,
) {
    match sample1.cmp(sample2) {
        std::cmp::Ordering::Less => println!("sample1 received first"),
        std::cmp::Ordering::Equal => println!("samples are equal"),
        std::cmp::Ordering::Greater => println!("sample2 received first"),
    }
}
```
</details>

---

## SampleContainer

`SampleContainer` is a fixed-capacity buffer passed to `Subscription::try_receive()` or `Subscription::receive()` to collect incoming samples. Its capacity must be specified at construction time, no heap allocation occurs during the receive call itself. Samples are held in insertion order and can be consumed with `pop_front()`.

**Key Points**:
- Samples are ordered by reception time and consumed with `pop_front()`
- Dropping a `SampleContainer` automatically releases all held samples
- Capacity should match or exceed the subscription's `max_sample_count` to avoid dropping samples
- Sample lifetime ties with `Subscription` instance.

### Batch Sample Processing

<details>
<summary>Examples:</summary>

```rust
use com_api::{SampleContainer, Subscription};
use com_api_gen::Tire;

// Process batch of samples
fn process_samples_batch(
    subscription: &impl Subscription<Tire>,
    batch_size: usize,
) -> Result<()> {
    let mut sample_container: SampleContainer<_> = SampleContainer::new(batch_size);

    loop {
        let num_new = subscription.try_receive(&mut sample_container, batch_size)?;

        if num_new == 0 {
            std::thread::sleep(std::time::Duration::from_millis(10));
            continue;
        }

        // Process each sample in the container
        while let Some(sample) = sample_container.pop_front() {
            println!("Tire pressure: {} psi", sample.pressure);
        }
    }
}
```
</details>

---

## Example

See [com-api-example](../../../example/com-api-example/) for a complete working example demonstrating:
- Service interface definition
- Producer offering and publishing events
- Consumer discovering and receiving events
- Error handling patterns

---
