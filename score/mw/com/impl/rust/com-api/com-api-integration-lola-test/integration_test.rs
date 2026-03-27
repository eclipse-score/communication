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

//! integration test module covering both primitive and user-defined payloads.
//!
//! Primitive types are validated through one combined `MixedPrimitivesPayload`
//! Complex user-defined types are validated through `ComplexStruct`
//! The tests ensure that the COM API correctly handles data transmission and reception
//! for both primitive and complex types when integrated with the Lola runtime.
#[cfg(test)]
mod integration_tests {
    use crate::generate_test;
    use crate::test_types::{
        ArrayStruct, ComplexStruct, ComplexStructInterface, MixedPrimitivesInterface,
        MixedPrimitivesPayload, NestedStruct, Point, Point3D, SensorData, SimpleStruct,
        VehicleState,
    };

    generate_test!(
        test = test_mixed_primitives_send_receive,
        interface = MixedPrimitivesInterface,
        event = mixed_event,
        path = "/IntegrationTest/MixedPrimitives",
        data = MixedPrimitivesPayload {
            u64_val: u64::MAX,
            i64_val: i64::MIN,
            u32_val: u32::MAX,
            i32_val: i32::MIN,
            f32_val: std::f32::consts::E,
            u16_val: u16::MAX,
            i16_val: i16::MIN,
            u8_val: u8::MAX,
            i8_val: i8::MIN,
            flag: true,
        },
        assert = |r: &MixedPrimitivesPayload, e: &MixedPrimitivesPayload| {
            assert_eq!(r.u64_val, e.u64_val, "u64_val mismatch");
            assert_eq!(r.i64_val, e.i64_val, "i64_val mismatch");
            assert_eq!(r.u32_val, e.u32_val, "u32_val mismatch");
            assert_eq!(r.i32_val, e.i32_val, "i32_val mismatch");
            assert!(
                (r.f32_val - e.f32_val).abs() < 1e-6_f32,
                "f32_val mismatch: {} vs {}",
                r.f32_val,
                e.f32_val
            );
            assert_eq!(r.u16_val, e.u16_val, "u16_val mismatch");
            assert_eq!(r.i16_val, e.i16_val, "i16_val mismatch");
            assert_eq!(r.u8_val, e.u8_val, "u8_val mismatch");
            assert_eq!(r.i8_val, e.i8_val, "i8_val mismatch");
            assert_eq!(r.flag, e.flag, "flag mismatch");
        },
    );

    generate_test!(
        test = test_complex_struct_send_receive,
        interface = ComplexStructInterface,
        event = complex_event,
        path = "/UserDefinedTest/ComplexStruct",
        data = ComplexStruct {
            count: 42,
            simple: SimpleStruct { id: 10 },
            nested: NestedStruct {
                id: 1,
                simple: SimpleStruct { id: 123 },
                value: 99.9
            },
            point: Point { x: 1.5, y: 2.5 },
            point3d: Point3D {
                x: 1.0,
                y: 2.0,
                z: 3.0
            },
            sensor: SensorData {
                sensor_id: 7,
                temperature: 22.5,
                humidity: 45.0,
                pressure: 1013.25
            },
            vehicle: VehicleState {
                speed: 60.0,
                rpm: 3000,
                fuel_level: 75.0,
                is_running: 1,
                mileage: 50_000
            },
            array: ArrayStruct {
                values: [1, 2, 3, 4, 5]
            }
        },
        assert = |r: &ComplexStruct, e: &ComplexStruct| {
            assert_eq!(r.count, e.count);
            assert_eq!(r.simple.id, e.simple.id);
            assert_eq!(r.nested.id, e.nested.id);
            assert_eq!(r.nested.simple.id, e.nested.simple.id);
            assert!((r.nested.value - e.nested.value).abs() < 0.01_f32);
            assert!((r.point.x - e.point.x).abs() < 0.001_f32);
            assert!((r.point.y - e.point.y).abs() < 0.001_f32);
            assert!((r.point3d.x - e.point3d.x).abs() < 0.001_f32);
            assert!((r.point3d.y - e.point3d.y).abs() < 0.001_f32);
            assert!((r.point3d.z - e.point3d.z).abs() < 0.001_f32);
            assert_eq!(r.sensor.sensor_id, e.sensor.sensor_id);
            assert!((r.sensor.temperature - e.sensor.temperature).abs() < 0.01_f32);
            assert!((r.sensor.humidity - e.sensor.humidity).abs() < 0.01_f32);
            assert!((r.sensor.pressure - e.sensor.pressure).abs() < 0.01_f32);
            assert!((r.vehicle.speed - e.vehicle.speed).abs() < 0.1_f32);
            assert_eq!(r.vehicle.rpm, e.vehicle.rpm);
            assert!((r.vehicle.fuel_level - e.vehicle.fuel_level).abs() < 0.1_f32);
            assert_eq!(r.vehicle.is_running, e.vehicle.is_running);
            assert_eq!(r.vehicle.mileage, e.vehicle.mileage);
            for i in 0..r.array.values.len() {
                assert_eq!(
                    r.array.values[i], e.array.values[i],
                    "array.values[{}] mismatch",
                    i
                );
            }
        },
    );
}
