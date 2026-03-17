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

//! Integration tests for primitive types
//! Tests all primitive types: u8, u16, u32, u64, i8, i16, i32, i64, f32, f64, bool

#[cfg(test)]
mod primitive_type_tests {
    use crate::test_types::{
        BoolData, F32Data, F64Data, I16Data, I32Data, I64Data, I8Data, PrimitiveConsumer,
        PrimitiveInterface, PrimitiveOfferedProducer, U16Data, U32Data, U64Data, U8Data,
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
    ) -> PrimitiveConsumer<R> {
        let consumer_discovery =
            runtime.find_service::<PrimitiveInterface>(FindServiceSpecifier::Specific(service_id));
        let available_service_instances = consumer_discovery.get_available_instances().unwrap();
        let consumer_builder = available_service_instances.into_iter().next().unwrap();
        consumer_builder.build().unwrap()
    }

    fn create_producer<R: Runtime>(
        runtime: &R,
        service_id: InstanceSpecifier,
    ) -> PrimitiveOfferedProducer<R> {
        let producer_builder = runtime.producer_builder::<PrimitiveInterface>(service_id);
        let producer = producer_builder.build().unwrap();
        producer.offer().unwrap()
    }

    #[test]
    fn test_u8_single_value_send_receive() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/U8/Single")
            .expect("Failed to create InstanceSpecifier");
        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);
        let test_value: u8 = 255;
        let u8_data_package = U8Data { value: test_value };

        let uninit_sample = producer.u8_event.allocate().unwrap();
        let sample = uninit_sample.write(u8_data_package);
        sample.send().unwrap();

        let subscription = consumer.u8_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1, "Should receive exactly 1 sample");
                let received_sample = sample_buf.pop_front().unwrap();
                assert_eq!(received_sample.value, test_value, "U8 values should match");
            }
            Err(e) => panic!("Failed to receive u8 data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_u8_multiple_values() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/U8/Multiple")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_values: Vec<u8> = vec![0, 128, 255, 42, 1];

        for &value in &test_values {
            let u8_data = U8Data { value };
            let uninit_sample = producer.u8_event.allocate().unwrap();
            let sample = uninit_sample.write(u8_data);
            sample.send().unwrap();
        }

        let subscription = consumer.u8_event.subscribe(5).unwrap();
        let mut sample_buf = SampleContainer::new(5);

        match subscription.try_receive(&mut sample_buf, 5) {
            Ok(count) => {
                assert_eq!(
                    count,
                    test_values.len(),
                    "Should receive {} samples",
                    test_values.len()
                );
                for (i, &expected_value) in test_values.iter().enumerate() {
                    if let Some(sample) = sample_buf.pop_front() {
                        assert_eq!(sample.value, expected_value, "Sample {} value mismatch", i);
                    } else {
                        panic!("Expected sample {} not found", i);
                    }
                }
            }
            Err(e) => panic!("Failed to receive u8 data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_u16_single_value_send_receive() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/U16/Single")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_value: u16 = u16::MAX;
        let u16_data = U16Data { value: test_value };

        let uninit_sample = producer.u16_event.allocate().unwrap();
        let sample = uninit_sample.write(u16_data);
        sample.send().unwrap();

        let subscription = consumer.u16_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1, "Should receive exactly 1 sample");
                let received_sample = sample_buf.pop_front().unwrap();
                assert_eq!(received_sample.value, test_value, "U16 values should match");
            }
            Err(e) => panic!("Failed to receive u16 data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_u16_multiple_values() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/U16/Multiple")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_values: Vec<u16> = vec![0, 256, 32768, 65535, 1000];

        for &value in &test_values {
            let u16_data = U16Data { value };
            let uninit_sample = producer.u16_event.allocate().unwrap();
            let sample = uninit_sample.write(u16_data);
            sample.send().unwrap();
        }

        let subscription = consumer.u16_event.subscribe(5).unwrap();
        let mut sample_buf = SampleContainer::new(5);

        match subscription.try_receive(&mut sample_buf, 5) {
            Ok(count) => {
                assert_eq!(count, test_values.len());
                for (i, &expected_value) in test_values.iter().enumerate() {
                    if let Some(sample) = sample_buf.pop_front() {
                        assert_eq!(sample.value, expected_value, "Sample {} value mismatch", i);
                    }
                }
            }
            Err(e) => panic!("Failed to receive u16 data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_u32_single_value_send_receive() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/U32/Single")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_value: u32 = u32::MAX;
        let u32_data = U32Data { value: test_value };

        let uninit_sample = producer.u32_event.allocate().unwrap();
        let sample = uninit_sample.write(u32_data);
        sample.send().unwrap();

        let subscription = consumer.u32_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1);
                let received_sample = sample_buf.pop_front().unwrap();
                assert_eq!(received_sample.value, test_value);
            }
            Err(e) => panic!("Failed to receive u32 data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_u64_single_value_send_receive() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/U64/Single")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_value: u64 = u64::MAX;
        let u64_data = U64Data { value: test_value };

        let uninit_sample = producer.u64_event.allocate().unwrap();
        let sample = uninit_sample.write(u64_data);
        sample.send().unwrap();

        let subscription = consumer.u64_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1);
                let received_sample = sample_buf.pop_front().unwrap();
                assert_eq!(received_sample.value, test_value);
            }
            Err(e) => panic!("Failed to receive u64 data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_i8_single_value_send_receive() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/I8/Single")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_value: i8 = -128;
        let i8_data = I8Data { value: test_value };

        let uninit_sample = producer.i8_event.allocate().unwrap();
        let sample = uninit_sample.write(i8_data);
        sample.send().unwrap();

        let subscription = consumer.i8_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1);
                let received_sample = sample_buf.pop_front().unwrap();
                assert_eq!(received_sample.value, test_value);
            }
            Err(e) => panic!("Failed to receive i8 data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_i8_negative_and_positive_values() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/I8/Multiple")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_values: Vec<i8> = vec![i8::MIN, -1, 0, 1, i8::MAX];

        for &value in &test_values {
            let i8_data = I8Data { value };
            let uninit_sample = producer.i8_event.allocate().unwrap();
            let sample = uninit_sample.write(i8_data);
            sample.send().unwrap();
        }

        let subscription = consumer.i8_event.subscribe(5).unwrap();
        let mut sample_buf = SampleContainer::new(5);

        match subscription.try_receive(&mut sample_buf, 5) {
            Ok(count) => {
                assert_eq!(count, test_values.len());
                for &expected_value in test_values.iter() {
                    if let Some(sample) = sample_buf.pop_front() {
                        assert_eq!(sample.value, expected_value);
                    }
                }
            }
            Err(e) => panic!("Failed to receive i8 data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_i16_single_value_send_receive() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/I16/Single")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_value: i16 = i16::MIN;
        let i16_data = I16Data { value: test_value };

        let uninit_sample = producer.i16_event.allocate().unwrap();
        let sample = uninit_sample.write(i16_data);
        sample.send().unwrap();

        let subscription = consumer.i16_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1);
                let received_sample = sample_buf.pop_front().unwrap();
                assert_eq!(received_sample.value, test_value);
            }
            Err(e) => panic!("Failed to receive i16 data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_i32_single_value_send_receive() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/I32/Single")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_value: i32 = i32::MIN;
        let i32_data = I32Data { value: test_value };

        let uninit_sample = producer.i32_event.allocate().unwrap();
        let sample = uninit_sample.write(i32_data);
        sample.send().unwrap();

        let subscription = consumer.i32_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1);
                let received_sample = sample_buf.pop_front().unwrap();
                assert_eq!(received_sample.value, test_value);
            }
            Err(e) => panic!("Failed to receive i32 data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_i64_single_value_send_receive() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/I64/Single")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_value: i64 = i64::MIN;
        let i64_data = I64Data { value: test_value };

        let uninit_sample = producer.i64_event.allocate().unwrap();
        let sample = uninit_sample.write(i64_data);
        sample.send().unwrap();

        let subscription = consumer.i64_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1);
                let received_sample = sample_buf.pop_front().unwrap();
                assert_eq!(received_sample.value, test_value);
            }
            Err(e) => panic!("Failed to receive i64 data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_f32_single_value_send_receive() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/F32/Single")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_value: f32 = 3.14159;
        let f32_data = F32Data { value: test_value };

        let uninit_sample = producer.f32_event.allocate().unwrap();
        let sample = uninit_sample.write(f32_data);
        sample.send().unwrap();

        let subscription = consumer.f32_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1);
                let received_sample = sample_buf.pop_front().unwrap();
                assert!((received_sample.value - test_value).abs() < 0.00001);
            }
            Err(e) => panic!("Failed to receive f32 data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_f32_various_values() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/F32/Multiple")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_values: Vec<f32> = vec![0.0, -1.5, 100.123, -999.999];

        for &value in &test_values {
            let f32_data = F32Data { value };
            let uninit_sample = producer.f32_event.allocate().unwrap();
            let sample = uninit_sample.write(f32_data);
            sample.send().unwrap();
        }

        let subscription = consumer.f32_event.subscribe(4).unwrap();
        let mut sample_buf = SampleContainer::new(4);

        match subscription.try_receive(&mut sample_buf, 4) {
            Ok(count) => {
                assert_eq!(count, test_values.len());
                for &expected_value in test_values.iter() {
                    if let Some(sample) = sample_buf.pop_front() {
                        assert!((sample.value - expected_value).abs() < 0.00001);
                    }
                }
            }
            Err(e) => panic!("Failed to receive f32 data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_f64_single_value_send_receive() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/F64/Single")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_value: f64 = 3.141592653589793;
        let f64_data = F64Data { value: test_value };

        let uninit_sample = producer.f64_event.allocate().unwrap();
        let sample = uninit_sample.write(f64_data);
        sample.send().unwrap();

        let subscription = consumer.f64_event.subscribe(1).unwrap();
        let mut sample_buf = SampleContainer::new(1);

        match subscription.try_receive(&mut sample_buf, 1) {
            Ok(count) => {
                assert_eq!(count, 1);
                let received_sample = sample_buf.pop_front().unwrap();
                assert!((received_sample.value - test_value).abs() < 0.0000001);
            }
            Err(e) => panic!("Failed to receive f64 data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }

    #[test]
    fn test_bool_send_receive() {
        let runtime = get_lola_runtime();
        let service_id = InstanceSpecifier::new("/PrimitiveTest/Bool/Single")
            .expect("Failed to create InstanceSpecifier");

        let producer = create_producer(&runtime, service_id.clone());
        let consumer = create_consumer(&runtime, service_id);

        let test_values: Vec<bool> = vec![true, false];

        for &value in &test_values {
            let bool_value = if value { true } else { false };
            let bool_data = BoolData { value: bool_value };
            let uninit_sample = producer.bool_event.allocate().unwrap();
            let sample = uninit_sample.write(bool_data);
            sample.send().unwrap();
        }

        let subscription = consumer.bool_event.subscribe(2).unwrap();
        let mut sample_buf = SampleContainer::new(2);

        match subscription.try_receive(&mut sample_buf, 2) {
            Ok(count) => {
                assert_eq!(count, test_values.len());
                for &expected_bool in test_values.iter() {
                    if let Some(sample) = sample_buf.pop_front() {
                        let received_bool = sample.value;
                        assert_eq!(received_bool, expected_bool);
                    }
                }
            }
            Err(e) => panic!("Failed to receive bool data: {:?}", e),
        }

        let _ = producer.unoffer().unwrap();
    }
}
