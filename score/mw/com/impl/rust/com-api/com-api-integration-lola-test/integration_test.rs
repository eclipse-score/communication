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

//! integration test module covering both primitive and user-defined payloads.
//!
//! Primitive types are validated through one combined `MixedPrimitivesPayload`

#[cfg(test)]
mod integration_tests {
    use crate::generate_test;
    use crate::test_types::{
        ComplexStruct, ComplexStructInterface, MixedPrimitivesInterface, MixedPrimitivesPayload,
        NestedStruct, NestedStructInterface, Point, Point3D, Point3DInterface, PointInterface,
        SensorData, SensorDataInterface, SimpleStruct, SimpleStructInterface, VehicleState,
        VehicleStateInterface,
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
        test = test_simple_struct_send_receive,
        interface = SimpleStructInterface,
        event = simple_event,
        path = "/UserDefinedTest/SimpleStruct",
        data = SimpleStruct { id: 42 },
        assert = |r: &SimpleStruct, e: &SimpleStruct| assert_eq!(r.id, e.id),
    );

    generate_test!(
        test = test_complex_struct_send_receive,
        interface = ComplexStructInterface,
        event = complex_event,
        path = "/UserDefinedTest/ComplexStruct",
        data = ComplexStruct {
            count: 42,
            temperature: 25.5,
            is_active: 1,
            timestamp: 1_234_567_890,
        },
        assert = |r: &ComplexStruct, e: &ComplexStruct| {
            assert_eq!(r.count, e.count);
            assert!((r.temperature - e.temperature).abs() < 0.01_f32);
            assert_eq!(r.is_active, e.is_active);
            assert_eq!(r.timestamp, e.timestamp);
        },
    );

    generate_test!(
        test = test_nested_struct_send_receive,
        interface = NestedStructInterface,
        event = nested_event,
        path = "/UserDefinedTest/NestedStruct",
        data = NestedStruct {
            id: 1,
            simple: SimpleStruct { id: 123 },
            value: 99.9,
        },
        assert = |r: &NestedStruct, e: &NestedStruct| {
            assert_eq!(r.id, e.id);
            assert_eq!(r.simple.id, e.simple.id);
            assert!((r.value - e.value).abs() < 0.01_f32);
        },
    );

    generate_test!(
        test = test_point_2d_send_receive,
        interface = PointInterface,
        event = point_event,
        path = "/UserDefinedTest/Point",
        data = Point { x: 1.5, y: 2.5 },
        assert = |r: &Point, e: &Point| {
            assert!((r.x - e.x).abs() < 0.001_f32);
            assert!((r.y - e.y).abs() < 0.001_f32);
        },
    );

    generate_test!(
        test = test_point_3d_send_receive,
        interface = Point3DInterface,
        event = point3d_event,
        path = "/UserDefinedTest/Point3D",
        data = Point3D {
            x: 1.0,
            y: 2.0,
            z: 3.0,
        },
        assert = |r: &Point3D, e: &Point3D| {
            assert!((r.x - e.x).abs() < 0.001_f32);
            assert!((r.y - e.y).abs() < 0.001_f32);
            assert!((r.z - e.z).abs() < 0.001_f32);
        },
    );

    generate_test!(
        test = test_sensor_data_send_receive,
        interface = SensorDataInterface,
        event = sensor_event,
        path = "/UserDefinedTest/SensorData",
        data = SensorData {
            sensor_id: 1,
            temperature: 22.5,
            humidity: 45.0,
            pressure: 1013.25,
        },
        assert = |r: &SensorData, e: &SensorData| {
            assert_eq!(r.sensor_id, e.sensor_id);
            assert!((r.temperature - e.temperature).abs() < 0.01_f32);
            assert!((r.humidity - e.humidity).abs() < 0.01_f32);
            assert!((r.pressure - e.pressure).abs() < 0.01_f32);
        },
    );

    generate_test!(
        test = test_vehicle_state_send_receive,
        interface = VehicleStateInterface,
        event = vehicle_event,
        path = "/UserDefinedTest/VehicleState",
        data = VehicleState {
            speed: 60.0,
            rpm: 3000,
            fuel_level: 75.0,
            is_running: 1,
            mileage: 50_000,
        },
        assert = |r: &VehicleState, e: &VehicleState| {
            assert!((r.speed - e.speed).abs() < 0.1_f32);
            assert_eq!(r.rpm, e.rpm);
            assert!((r.fuel_level - e.fuel_level).abs() < 0.1_f32);
            assert_eq!(r.is_running, e.is_running);
            assert_eq!(r.mileage, e.mileage);
        },
    );
}
