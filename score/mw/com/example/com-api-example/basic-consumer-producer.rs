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

use com_api::{Runtime, Subscriber, Result, SampleContainer, Producer, Publisher, Subscription, Error,
    SampleMaybeUninit, SampleMut, FindServiceSpecifier, ServiceDiscovery, ConsumerDescriptor, Builder,
    InstanceSpecifier, MockRuntimeBuilderImpl, MockRuntimeImpl, LolaRuntimeBuilderImpl, LolaRuntimeImpl};

use com_api_gen::{VehicleConsumer, VehicleOfferedProducer, Tire, VehicleInterface};

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
        let mut sample_buf = SampleContainer::new();

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
        let uninit_sample = self.producer.left_tire.allocate()?;
        let sample = uninit_sample.write(tire);
        sample.send()?;
        Ok(())
    }
}

fn use_consumer<R: Runtime>(runtime: &R) -> VehicleConsumer<R> {
    // Find all the avaiable service instances using ANY specifier
    let consumer_discovery = runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Any);
    let available_service_instances = consumer_discovery.get_available_instances().unwrap();

    let consumer_builder = available_service_instances
        .into_iter()
        .find(|desc| desc.get_instance_identifier().as_ref() == "/My/Funk/ServiceName")
        .unwrap();

    consumer_builder.build().unwrap()
}

fn use_producer<R: Runtime>(runtime: &R) -> VehicleOfferedProducer<R> {
    let producer_builder = runtime.producer_builder::<VehicleInterface>(
        InstanceSpecifier::new("/My/Funk/ServiceName").unwrap(),
    );
    let producer = producer_builder.build().unwrap();
    producer.offer().unwrap()
}

fn run_with_runtime<R: Runtime>(name: &str, runtime: &R) {
    println!("\n=== Running with {name} runtime ===");
    let producer = use_producer(runtime);
    let consumer = use_consumer(runtime);
    let monitor = VehicleMonitor::new(consumer, producer).unwrap();

    for _ in 0..5 {
        monitor.write_tire_data(Tire {}).unwrap();
        let tire_data = monitor.read_tire_data().unwrap();
        println!("{tire_data}");
    }
    println!("=== {name} runtime completed ===\n");
}

fn main() {
    let mock_runtime_builder = MockRuntimeBuilderImpl::new();
    let mock_runtime = Builder::<MockRuntimeImpl>::build(mock_runtime_builder).unwrap();
    run_with_runtime("Mock", &mock_runtime);

    let lola_runtime_builder = LolaRuntimeBuilderImpl::new();
    let lola_runtime = Builder::<LolaRuntimeImpl>::build(lola_runtime_builder).unwrap();
    run_with_runtime("Lola", &lola_runtime);
}

#[cfg(test)]
mod test {

    use super::*;
    #[test]
    fn create_producer() {
        // Factory
        let mock_runtime_builder = MockRuntimeBuilderImpl::new();
        let runtime = Builder::<MockRuntimeImpl>::build(mock_runtime_builder).unwrap();
        use_producer(&runtime);

        let lola_runtime_builder = LolaRuntimeBuilderImpl::new();
        let runtime = Builder::<LolaRuntimeImpl>::build(lola_runtime_builder).unwrap();
        use_producer(&runtime);
    }

    #[test]
    fn create_consumer() {
        let mock_runtime_builder = MockRuntimeBuilderImpl::new();
        let runtime = Builder::<MockRuntimeImpl>::build(mock_runtime_builder).unwrap();
        use_consumer(&runtime);

        let lola_runtime_builder = LolaRuntimeBuilderImpl::new();
        let runtime = Builder::<LolaRuntimeImpl>::build(lola_runtime_builder).unwrap();
        use_consumer(&runtime);
    }

    async fn async_data_processor_fn<R: Runtime>(subscribed: impl Subscription<Tire, R>) {
        let mut buffer = SampleContainer::new();
        for _ in 0..10 {
            match subscribed.receive(&mut buffer, 1, 1).await {
                Ok(0) => panic!("No sample received"),
                Ok(num_samples) => {
                    println!(
                        "{} samples received: sample[0] = {:?}",
                        num_samples,
                        *buffer.front().unwrap()
                    );
                }
                Err(e) => panic!("{:?}", e),
            }
        }
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn schedule_subscription_on_mt_scheduler() {
        let mock_runtime_builder = MockRuntimeBuilderImpl::new();
        let runtime = Builder::<MockRuntimeImpl>::build(mock_runtime_builder).unwrap();

        let consumer_discovery =
            runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Any);
        let available_service_instances = consumer_discovery.get_available_instances().unwrap();

        // Create consumer from first discovered service
        let consumer_builder = available_service_instances
            .into_iter()
            .find(|desc| desc.get_instance_identifier().as_ref() == "/My/Funk/ServiceName")
            .unwrap();
        let consumer = consumer_builder.build().unwrap();

        // Subscribe to one event
        let subscribed = consumer.left_tire.subscribe(3).unwrap();

        tokio::spawn(async_data_processor_fn(subscribed))
            .await
            .expect("Error returned from task");
    }
}
