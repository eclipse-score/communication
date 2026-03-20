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

//! Test types and interfaces for integration testing.
use com_api::{interface, CommData, ProviderInfo, Publisher, Reloc, Subscriber};

// Macro to generate data types with CommData impl with Eq trait
macro_rules! define_type {
    ($name:ident, eq, $($field:ident: $field_type:ty),+) => {
        #[derive(Debug, Clone, Copy, PartialEq, Eq, Reloc)]
        #[repr(C)]
        pub struct $name {
            $(pub $field: $field_type,)*
        }
        impl CommData for $name {
            const ID: &'static str = stringify!($name);
        }
    };
    // Macro for types without Eq trait (e.g., with float fields)
    ($name:ident, $($field:ident: $field_type:ty),+) => {
        #[derive(Debug, Clone, Copy, PartialEq, Reloc)]
        #[repr(C)]
        pub struct $name {
            $(pub $field: $field_type,)*
        }
        impl CommData for $name {
            const ID: &'static str = stringify!($name);
        }
    };
}

define_type!(U8Data, eq, value: u8);
define_type!(U16Data, eq, value: u16);
define_type!(U32Data, eq, value: u32);
define_type!(U64Data, eq, value: u64);
define_type!(I8Data, eq, value: i8);
define_type!(I16Data, eq, value: i16);
define_type!(I32Data, eq, value: i32);
define_type!(I64Data, eq, value: i64);
define_type!(BoolData, eq, value: bool);

// Floating point types (without Eq)
define_type!(F32Data, value: f32);
define_type!(F64Data, value: f64);

// Complex struct types
define_type!(SimpleStruct, eq, id: u32);
define_type!(ComplexStruct, count: u32, temperature: f32, is_active: u8, timestamp: u64);
define_type!(NestedStruct, id: u32, simple: SimpleStruct, value: f32);
define_type!(Point, x: f32, y: f32);
define_type!(Point3D, x: f32, y: f32, z: f32);
define_type!(SensorData, sensor_id: u16, temperature: f32, humidity: f32, pressure: f32);
define_type!(VehicleState, speed: f32, rpm: u16, fuel_level: f32, is_running: u8, mileage: u32);

interface!(interface U8,   { Id = "U8Interface",   u8_event:   Event<U8Data>   });
interface!(interface U16,  { Id = "U16Interface",  u16_event:  Event<U16Data>  });
interface!(interface U32,  { Id = "U32Interface",  u32_event:  Event<U32Data>  });
interface!(interface U64,  { Id = "U64Interface",  u64_event:  Event<U64Data>  });
interface!(interface I8,   { Id = "I8Interface",   i8_event:   Event<I8Data>   });
interface!(interface I16,  { Id = "I16Interface",  i16_event:  Event<I16Data>  });
interface!(interface I32,  { Id = "I32Interface",  i32_event:  Event<I32Data>  });
interface!(interface I64,  { Id = "I64Interface",  i64_event:  Event<I64Data>  });
interface!(interface F32,  { Id = "F32Interface",  f32_event:  Event<F32Data>  });
interface!(interface F64,  { Id = "F64Interface",  f64_event:  Event<F64Data>  });
interface!(interface Bool, { Id = "BoolInterface", bool_event: Event<BoolData> });

interface!(interface SimpleStruct,  { Id = "SimpleStructInterface",  simple_event:  Event<SimpleStruct>  });
interface!(interface ComplexStruct, { Id = "ComplexStructInterface", complex_event: Event<ComplexStruct> });
interface!(interface NestedStruct,  { Id = "NestedStructInterface",  nested_event:  Event<NestedStruct>  });
interface!(interface Point,         { Id = "PointInterface",         point_event:   Event<Point>         });
interface!(interface Point3D,       { Id = "Point3DInterface",       point3d_event: Event<Point3D>       });
interface!(interface SensorData,    { Id = "SensorDataInterface",    sensor_event:  Event<SensorData>    });
interface!(interface VehicleState,  { Id = "VehicleStateInterface",  vehicle_event: Event<VehicleState>  });

// Generic test macro for send/receive tests
#[macro_export]
macro_rules! generate_test {
    (
        test      = $test_fn:ident,
        interface = $Iface:ident,
        event     = $event_field:ident,
        path      = $path:literal,
        data      = $test_data:expr,
        assert    = $assert:expr $(,)?
    ) => {
        #[test]
        fn $test_fn() {
            use com_api::{
                Builder, FindServiceSpecifier, InstanceSpecifier, LolaRuntimeBuilderImpl,
                OfferedProducer, Producer, Publisher, Runtime, RuntimeBuilder, SampleContainer,
                SampleMaybeUninit, SampleMut, ServiceDiscovery, Subscriber, Subscription,
            };

            let mut builder = LolaRuntimeBuilderImpl::new();
            builder.load_config(std::path::Path::new(
                "score/mw/com/impl/rust/com-api/com-api-integration-lola-test/config.json",
            ));
            let runtime = builder.build().expect("Failed to build runtime");

            let service_id =
                InstanceSpecifier::new($path).expect("Failed to create InstanceSpecifier");

            let producer = runtime
                .producer_builder::<$Iface>(service_id.clone())
                .build()
                .expect("Failed to build producer")
                .offer()
                .expect("Failed to offer producer");
            let consumer = runtime
                .find_service::<$Iface>(FindServiceSpecifier::Specific(service_id))
                .get_available_instances()
                .expect("Failed to get available instances")
                .into_iter()
                .next()
                .expect("No service instances available")
                .build()
                .expect("Failed to build consumer");

            let test_value = $test_data;
            let uninit_sample = producer
                .$event_field
                .allocate()
                .expect("Failed to allocate sample");
            let sample = uninit_sample.write(test_value);
            sample.send().expect("Failed to send sample");

            let subscription = consumer
                .$event_field
                .subscribe(1)
                .expect("Failed to subscribe to event");
            let mut sample_buf = SampleContainer::new(1);

            match subscription.try_receive(&mut sample_buf, 1) {
                Ok(received_count) => {
                    assert_eq!(received_count, 1, "Should receive exactly 1 sample");
                    let received = sample_buf.pop_front().unwrap();
                    ($assert)(&*received, &test_value);
                }
                Err(e) => panic!("Failed to receive data: {:?}", e),
            }

            let _ = producer.unoffer().expect("Failed to unoffer producer");
        }
    };
}
