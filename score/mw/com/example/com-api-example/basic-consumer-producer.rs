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

use com_api::{
    Builder, Error, FindServiceSpecifier, InstanceSpecifier, LolaRuntimeBuilderImpl, Producer,
    Publisher, Result, Runtime, RuntimeBuilder, SampleContainer, SampleMaybeUninit, SampleMut,
    ServiceDiscovery, Subscriber, Subscription,
};

use com_api_gen::{Tire, VehicleConsumer, VehicleInterface, VehicleOfferedProducer};

// Example struct demonstrating composition with VehicleConsumer
pub struct VehicleMonitor<R: Runtime> {
    _consumer: VehicleConsumer<R>,
    producer: VehicleOfferedProducer<R>,
    tire_subscriber: <<R as Runtime>::Subscriber<Tire> as Subscriber<Tire, R>>::Subscription,
}

impl<R: Runtime> VehicleMonitor<R> {
    /// Create a new `VehicleMonitor` with a consumer
    ///
    /// # Returns
    ///
    /// A `Result` containing the monitor on success or an error if subscription fails.
    ///
    /// # Errors
    ///
    /// Returns an error if subscribing to the tire data fails.
    ///
    /// # Panics
    ///
    /// Panics if unwrapping the subscription fails.
    pub fn new(consumer: VehicleConsumer<R>, producer: VehicleOfferedProducer<R>) -> Result<Self> {
        let tire_subscriber = consumer.left_tire.subscribe(3).unwrap();
        Ok(Self {
            _consumer: consumer,
            producer,
            tire_subscriber,
        })
    }

    /// Monitor tire data from the consumer
    ///
    /// # Returns
    ///
    /// A `Result` containing tire data as a `String` on success or an error if reading fails.
    ///
    /// # Errors
    ///
    /// Returns an error if no data is received or if there is a failure in receiving data
    ///
    /// # Panics
    ///
    /// Panics if unwrapping the sample from the buffer fails.
    pub fn read_tire_data(&self) -> Result<String> {
        let mut sample_buf = SampleContainer::new(3);

        match self.tire_subscriber.try_receive(&mut sample_buf, 1) {
            Ok(0) => Err(Error::Fail),
            Ok(x) => {
                let sample = sample_buf.pop_front().unwrap();
                Ok(format!("{} samples received: sample[0] = {:?}", x, *sample))
            }
            Err(e) => Err(e),
        }
    }

    /// Write tire data using the producer
    /// # Returns
    ///
    /// A `Result` indicating success or failure of the write operation.
    ///
    /// # Errors
    ///
    /// Returns an error if allocation or sending of the sample fails.
    pub fn write_tire_data(&self, tire: Tire) -> Result<()> {
        // Allocate API of lola is not calling at the moment
        let uninit_sample = self.producer.left_tire.allocate()?;
        let sample = uninit_sample.write(tire);
        sample.send()?;
        println!("Tire data sent");
        Ok(())
    }
}

fn create_consumer<R: Runtime>(runtime: &R, service_id: InstanceSpecifier) -> VehicleConsumer<R> {
    // Find all the avaiable service instances using ANY specifier
    let consumer_discovery =
        runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(service_id));
    let available_service_instances = consumer_discovery.get_available_instances().unwrap();

    // Select service instance at specific handle_index
    let handle_index = 0; // or any index you need from vector of instances
    let consumer_builder = available_service_instances
        .into_iter()
        .nth(handle_index)
        .unwrap();

    consumer_builder.build().unwrap()
}

fn create_producer<R: Runtime>(
    runtime: &R,
    service_id: InstanceSpecifier,
) -> VehicleOfferedProducer<R> {
    let producer_builder = runtime.producer_builder::<VehicleInterface>(service_id);
    let producer = producer_builder.build().unwrap();
    producer.offer().unwrap()
}

fn run_with_runtime<R: Runtime>(name: &str, runtime: &R) {
    println!("\n=== Running with {name} runtime ===");
    let producer = create_producer(
        runtime,
        InstanceSpecifier::new("/Vehicle/Service/Instance")
            .expect("Failed to create InstanceSpecifier"),
    );
    let consumer = create_consumer(
        runtime,
        InstanceSpecifier::new("/Vehicle/Service/Instance")
            .expect("Failed to create InstanceSpecifier"),
    );
    let monitor = VehicleMonitor::new(consumer, producer).unwrap();
    let tire_pressure = 5.0;
    println!("Setting tire pressure to {tire_pressure}");
    for i in 0..5 {
        monitor
            .write_tire_data(Tire {
                pressure: tire_pressure + i as f32,
            })
            .unwrap();
        let tire_data = monitor.read_tire_data().unwrap();
        println!("{tire_data}");
    }
    println!("=== {name} runtime completed ===\n");
}

fn init_lola_runtime_builder() -> LolaRuntimeBuilderImpl {
    let mut lola_runtime_builder = LolaRuntimeBuilderImpl::new();
    lola_runtime_builder.load_config(std::path::Path::new(
        "score/mw/com/example/com-api-example/etc/config.json",
    ));
    lola_runtime_builder
}

fn main() {
    let lola_runtime_builder = init_lola_runtime_builder();
    let lola_runtime = lola_runtime_builder.build().unwrap();
    run_with_runtime("Lola", &lola_runtime);
}

#[cfg(test)]
mod test {

    use super::*;
    #[test]
    fn integration_test() {
        let lola_runtime_builder = init_lola_runtime_builder();
        let lola_runtime = lola_runtime_builder.build().unwrap();
        run_with_runtime("Lola", &lola_runtime);
    }
}
