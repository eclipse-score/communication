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

//! Integration tests for user-defined types
//! Tests complex data structures including simple structs, nested structs, and composite types

#[cfg(test)]
mod user_defined_type_tests {
    use crate::test_types::{
        ComplexStruct, NestedStruct, Point, Point3D, SensorData, SimpleStruct, UserDefinedConsumer,
        UserDefinedInterface, UserDefinedOfferedProducer, VehicleState,
    };
    use com_api::{
        Builder, FindServiceSpecifier, InstanceSpecifier, LolaRuntimeBuilderImpl, OfferedProducer,
        Producer, Publisher, Runtime, RuntimeBuilder, SampleContainer, SampleMaybeUninit,
        SampleMut, ServiceDiscovery, Subscriber, Subscription,
    };

    fn get_lola_runtime() -> impl Runtime {
        let mut builder = LolaRuntimeBuilderImpl::new();
        builder.load_config(std::path::Path::new(
            "score/mw/com/impl/rust/com-api/com-api-integration-lola-test/config.json",
        ));
        builder.build().expect("Failed to build runtime")
    }

    fn create_consumer<R: Runtime>(
        runtime: &R,
        service_id: InstanceSpecifier,
    ) -> UserDefinedConsumer<R> {
        let consumer_discovery = runtime
            .find_service::<UserDefinedInterface>(FindServiceSpecifier::Specific(service_id));
        let available_service_instances = consumer_discovery.get_available_instances().unwrap();
        let consumer_builder = available_service_instances.into_iter().next().unwrap();
        consumer_builder.build().unwrap()
    }

    fn create_producer<R: Runtime>(
        runtime: &R,
        service_id: InstanceSpecifier,
    ) -> UserDefinedOfferedProducer<R> {
        let producer_builder = runtime.producer_builder::<UserDefinedInterface>(service_id);
        let producer = producer_builder.build().unwrap();
        producer.offer().unwrap()
    }

    #[test]
    fn test_simple_struct_single_value() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/UserDefinedTest/SimpleStruct/Instance1")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_data = SimpleStruct { id: 42 };

        let uninit_sample = producer.simple_event.allocate().unwrap();
        let sample = uninit_sample.write(test_data);
        sample.send().unwrap();

        let subscription = consumer.simple_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1, "Should receive exactly 1 sample");
                let received_sample = sample_buf.pop_front().unwrap();
                assert_eq!(received_sample.id, 42, "SimpleStruct ID should match");
            }
            Err(e) => panic!("Failed to receive SimpleStruct data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_simple_struct_multiple_values() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/UserDefinedTest/SimpleStruct/Instance2")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_data = vec![
            SimpleStruct { id: 1 },
            SimpleStruct { id: 100 },
            SimpleStruct { id: 9999 },
            SimpleStruct { id: 0 },
            SimpleStruct { id: u32::MAX },
        ];

        for data in &test_data {
            let uninit_sample = producer.simple_event.allocate().unwrap();
            let sample = uninit_sample.write(*data);
            sample.send().unwrap();
        }

        let subscription = consumer.simple_event.subscribe(5).unwrap();
        let mut sample_buf = SampleContainer::new(5);

        match subscription.try_receive(&mut sample_buf, 5) {
            Ok(count) => {
                assert_eq!(count, test_data.len());
                for (i, expected) in test_data.iter().enumerate() {
                    if let Some(sample) = sample_buf.pop_front() {
                        assert_eq!(
                            sample.id, expected.id,
                            "SimpleStruct mismatch at index {}",
                            i
                        );
                    }
                }
            }
            Err(e) => panic!("Failed to receive SimpleStruct data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_complex_struct_single_value() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/UserDefinedTest/ComplexStruct/Instance1")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_data = ComplexStruct {
            count: 42,
            temperature: 25.5,
            is_active: 1,
            timestamp: 1234567890,
        };

        let uninit_sample = producer.complex_event.allocate().unwrap();
        let sample = uninit_sample.write(test_data);
        sample.send().unwrap();

        let subscription = consumer.complex_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1);
                let received = sample_buf.pop_front().unwrap();
                assert_eq!(received.count, 42);
                assert!((received.temperature - 25.5).abs() < 0.01);
                assert_eq!(received.is_active, 1);
                assert_eq!(received.timestamp, 1234567890);
            }
            Err(e) => panic!("Failed to receive ComplexStruct data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_complex_struct_multiple_values() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/UserDefinedTest/ComplexStruct/Instance2")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_data = vec![
            ComplexStruct {
                count: 0,
                temperature: 0.0,
                is_active: 0,
                timestamp: 0,
            },
            ComplexStruct {
                count: 100,
                temperature: 20.0,
                is_active: 1,
                timestamp: 1000000,
            },
            ComplexStruct {
                count: u32::MAX,
                temperature: -40.5,
                is_active: 0,
                timestamp: u64::MAX,
            },
        ];

        for data in &test_data {
            let uninit_sample = producer.complex_event.allocate().unwrap();
            let sample = uninit_sample.write(*data);
            sample.send().unwrap();
        }

        let subscription = consumer.complex_event.subscribe(3).unwrap();
        let mut sample_buf = SampleContainer::new(3);

        match subscription.try_receive(&mut sample_buf, 3) {
            Ok(count) => {
                assert_eq!(count, test_data.len());
                for expected in test_data.iter() {
                    if let Some(received) = sample_buf.pop_front() {
                        assert_eq!(received.count, expected.count);
                        assert!((received.temperature - expected.temperature).abs() < 0.01);
                        assert_eq!(received.is_active, expected.is_active);
                        assert_eq!(received.timestamp, expected.timestamp);
                    }
                }
            }
            Err(e) => panic!("Failed to receive ComplexStruct data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_nested_struct_single_value() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/UserDefinedTest/NestedStruct/Instance1")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let inner_struct = SimpleStruct { id: 123 };
        let test_data = NestedStruct {
            id: 1,
            simple: inner_struct,
            value: 99.9,
        };

        let uninit_sample = producer.nested_event.allocate().unwrap();
        let sample = uninit_sample.write(test_data);
        sample.send().unwrap();

        let subscription = consumer.nested_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1);
                let received = sample_buf.pop_front().unwrap();
                assert_eq!(received.id, 1);
                assert_eq!(received.simple.id, 123);
                assert!((received.value - 99.9).abs() < 0.01);
            }
            Err(e) => panic!("Failed to receive NestedStruct data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_nested_struct_multiple_values() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/UserDefinedTest/NestedStruct/Instance2")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_data = vec![
            NestedStruct {
                id: 1,
                simple: SimpleStruct { id: 10 },
                value: 1.1,
            },
            NestedStruct {
                id: 2,
                simple: SimpleStruct { id: 20 },
                value: 2.2,
            },
            NestedStruct {
                id: 3,
                simple: SimpleStruct { id: 30 },
                value: 3.3,
            },
        ];

        for data in &test_data {
            let uninit_sample = producer.nested_event.allocate().unwrap();
            let sample = uninit_sample.write(*data);
            sample.send().unwrap();
        }

        let subscription = consumer.nested_event.subscribe(3).unwrap();
        let mut sample_buf = SampleContainer::new(3);

        match subscription.try_receive(&mut sample_buf, 3) {
            Ok(count) => {
                assert_eq!(count, test_data.len());
                for expected in test_data.iter() {
                    if let Some(received) = sample_buf.pop_front() {
                        assert_eq!(received.id, expected.id);
                        assert_eq!(received.simple.id, expected.simple.id);
                        assert!((received.value - expected.value).abs() < 0.01);
                    }
                }
            }
            Err(e) => panic!("Failed to receive NestedStruct data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_point_2d_single_value() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/UserDefinedTest/Point/Instance1")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_data = Point { x: 1.5, y: 2.5 };

        let uninit_sample = producer.point_event.allocate().unwrap();
        let sample = uninit_sample.write(test_data);
        sample.send().unwrap();

        let subscription = consumer.point_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1);
                let received = sample_buf.pop_front().unwrap();
                assert!((received.x - 1.5).abs() < 0.001);
                assert!((received.y - 2.5).abs() < 0.001);
            }
            Err(e) => panic!("Failed to receive Point data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_point_2d_multiple_values() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/UserDefinedTest/Point/Instance2")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_data = vec![
            Point { x: 0.0, y: 0.0 },
            Point { x: 1.0, y: 1.0 },
            Point { x: -1.5, y: 2.5 },
            Point {
                x: 100.0,
                y: -100.0,
            },
        ];

        for data in &test_data {
            let uninit_sample = producer.point_event.allocate().unwrap();
            let sample = uninit_sample.write(*data);
            sample.send().unwrap();
        }

        let subscription = consumer.point_event.subscribe(4).unwrap();
        let mut sample_buf = SampleContainer::new(4);

        match subscription.try_receive(&mut sample_buf, 4) {
            Ok(count) => {
                assert_eq!(count, test_data.len());
                for expected in test_data.iter() {
                    if let Some(received) = sample_buf.pop_front() {
                        assert!((received.x - expected.x).abs() < 0.001);
                        assert!((received.y - expected.y).abs() < 0.001);
                    }
                }
            }
            Err(e) => panic!("Failed to receive Point data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_point_3d_single_value() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/UserDefinedTest/Point3D/Instance1")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_data = Point3D {
            x: 1.0,
            y: 2.0,
            z: 3.0,
        };

        let uninit_sample = producer.point3d_event.allocate().unwrap();
        let sample = uninit_sample.write(test_data);
        sample.send().unwrap();

        let subscription = consumer.point3d_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1);
                let received = sample_buf.pop_front().unwrap();
                assert!((received.x - 1.0).abs() < 0.001);
                assert!((received.y - 2.0).abs() < 0.001);
                assert!((received.z - 3.0).abs() < 0.001);
            }
            Err(e) => panic!("Failed to receive Point3D data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_point_3d_multiple_values() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/UserDefinedTest/Point3D/Instance2")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_data = vec![
            Point3D {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            Point3D {
                x: 1.0,
                y: 2.0,
                z: 3.0,
            },
            Point3D {
                x: -5.5,
                y: -6.6,
                z: -7.7,
            },
        ];

        for data in &test_data {
            let uninit_sample = producer.point3d_event.allocate().unwrap();
            let sample = uninit_sample.write(*data);
            sample.send().unwrap();
        }

        let subscription = consumer.point3d_event.subscribe(3).unwrap();
        let mut sample_buf = SampleContainer::new(3);

        match subscription.try_receive(&mut sample_buf, 3) {
            Ok(count) => {
                assert_eq!(count, test_data.len());
                for expected in test_data.iter() {
                    if let Some(received) = sample_buf.pop_front() {
                        assert!((received.x - expected.x).abs() < 0.001);
                        assert!((received.y - expected.y).abs() < 0.001);
                        assert!((received.z - expected.z).abs() < 0.001);
                    }
                }
            }
            Err(e) => panic!("Failed to receive Point3D data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_sensor_data_single_value() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/UserDefinedTest/SensorData/Instance1")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_data = SensorData {
            sensor_id: 1,
            temperature: 22.5,
            humidity: 45.0,
            pressure: 1013.25,
        };

        let uninit_sample = producer.sensor_event.allocate().unwrap();
        let sample = uninit_sample.write(test_data);
        sample.send().unwrap();

        let subscription = consumer.sensor_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1);
                let received = sample_buf.pop_front().unwrap();
                assert_eq!(received.sensor_id, 1);
                assert!((received.temperature - 22.5).abs() < 0.01);
                assert!((received.humidity - 45.0).abs() < 0.01);
                assert!((received.pressure - 1013.25).abs() < 0.01);
            }
            Err(e) => panic!("Failed to receive SensorData: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_sensor_data_multiple_values() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/UserDefinedTest/SensorData/Instance2")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_data = vec![
            SensorData {
                sensor_id: 1,
                temperature: 20.0,
                humidity: 50.0,
                pressure: 1013.0,
            },
            SensorData {
                sensor_id: 2,
                temperature: 25.0,
                humidity: 40.0,
                pressure: 1014.0,
            },
            SensorData {
                sensor_id: 3,
                temperature: 15.0,
                humidity: 60.0,
                pressure: 1012.0,
            },
        ];

        for data in &test_data {
            let uninit_sample = producer.sensor_event.allocate().unwrap();
            let sample = uninit_sample.write(*data);
            sample.send().unwrap();
        }

        let subscription = consumer.sensor_event.subscribe(3).unwrap();
        let mut sample_buf = SampleContainer::new(3);

        match subscription.try_receive(&mut sample_buf, 3) {
            Ok(count) => {
                assert_eq!(count, test_data.len());
                for expected in test_data.iter() {
                    if let Some(received) = sample_buf.pop_front() {
                        assert_eq!(received.sensor_id, expected.sensor_id);
                        assert!((received.temperature - expected.temperature).abs() < 0.01);
                        assert!((received.humidity - expected.humidity).abs() < 0.01);
                        assert!((received.pressure - expected.pressure).abs() < 0.01);
                    }
                }
            }
            Err(e) => panic!("Failed to receive SensorData: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_vehicle_state_single_value() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/UserDefinedTest/VehicleState/Instance1")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_data = VehicleState {
            speed: 60.0,
            rpm: 3000,
            fuel_level: 75.0,
            is_running: 1,
            mileage: 50000,
        };

        let uninit_sample = producer.vehicle_event.allocate().unwrap();
        let sample = uninit_sample.write(test_data);
        sample.send().unwrap();

        let subscription = consumer.vehicle_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1);
                let received = sample_buf.pop_front().unwrap();
                assert!((received.speed - 60.0).abs() < 0.1);
                assert_eq!(received.rpm, 3000);
                assert!((received.fuel_level - 75.0).abs() < 0.1);
                assert_eq!(received.is_running, 1);
                assert_eq!(received.mileage, 50000);
            }
            Err(e) => panic!("Failed to receive VehicleState: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_vehicle_state_multiple_values() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/UserDefinedTest/VehicleState/Instance2")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_data = vec![
            VehicleState {
                speed: 0.0,
                rpm: 0,
                fuel_level: 100.0,
                is_running: 0,
                mileage: 0,
            },
            VehicleState {
                speed: 50.0,
                rpm: 2000,
                fuel_level: 90.0,
                is_running: 1,
                mileage: 10000,
            },
            VehicleState {
                speed: 120.0,
                rpm: 5000,
                fuel_level: 25.0,
                is_running: 1,
                mileage: 100000,
            },
            VehicleState {
                speed: 0.0,
                rpm: 1000,
                fuel_level: 50.0,
                is_running: 1,
                mileage: 150000,
            },
        ];

        for data in &test_data {
            let uninit_sample = producer.vehicle_event.allocate().unwrap();
            let sample = uninit_sample.write(*data);
            sample.send().unwrap();
        }

        let subscription = consumer.vehicle_event.subscribe(4).unwrap();
        let mut sample_buf = SampleContainer::new(4);

        match subscription.try_receive(&mut sample_buf, 4) {
            Ok(count) => {
                assert_eq!(count, test_data.len());
                for expected in test_data.iter() {
                    if let Some(received) = sample_buf.pop_front() {
                        assert!((received.speed - expected.speed).abs() < 0.1);
                        assert_eq!(received.rpm, expected.rpm);
                        assert!((received.fuel_level - expected.fuel_level).abs() < 0.1);
                        assert_eq!(received.is_running, expected.is_running);
                        assert_eq!(received.mileage, expected.mileage);
                    }
                }
            }
            Err(e) => panic!("Failed to receive VehicleState: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }
}
