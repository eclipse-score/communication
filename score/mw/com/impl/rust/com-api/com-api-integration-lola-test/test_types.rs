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

//! Test types and interfaces for integration testing
//! This module defines various data types and interfaces used for testing
//! both primitive types and user-defined types in the COM API.

use com_api::{
    CommData, Consumer, Interface, OfferedProducer, Producer, ProviderInfo, Publisher, Reloc,
    Runtime, Subscriber,
};

/// Test type for u8 primitive
#[derive(Debug, Clone, Copy, PartialEq, Eq, Reloc)]
#[repr(C)]
pub struct U8Data {
    pub value: u8,
}

impl CommData for U8Data {
    const ID: &'static str = "U8Data";
}

/// Test type for u16 primitive
#[derive(Debug, Clone, Copy, PartialEq, Eq, Reloc)]
#[repr(C)]
pub struct U16Data {
    pub value: u16,
}

impl CommData for U16Data {
    const ID: &'static str = "U16Data";
}

/// Test type for u32 primitive
#[derive(Debug, Clone, Copy, PartialEq, Eq, Reloc)]
#[repr(C)]
pub struct U32Data {
    pub value: u32,
}

impl CommData for U32Data {
    const ID: &'static str = "U32Data";
}

/// Test type for u64 primitive
#[derive(Debug, Clone, Copy, PartialEq, Eq, Reloc)]
#[repr(C)]
pub struct U64Data {
    pub value: u64,
}

impl CommData for U64Data {
    const ID: &'static str = "U64Data";
}

/// Test type for i8 primitive
#[derive(Debug, Clone, Copy, PartialEq, Eq, Reloc)]
#[repr(C)]
pub struct I8Data {
    pub value: i8,
}

impl CommData for I8Data {
    const ID: &'static str = "I8Data";
}

/// Test type for i16 primitive
#[derive(Debug, Clone, Copy, PartialEq, Eq, Reloc)]
#[repr(C)]
pub struct I16Data {
    pub value: i16,
}

impl CommData for I16Data {
    const ID: &'static str = "I16Data";
}

/// Test type for i32 primitive
#[derive(Debug, Clone, Copy, PartialEq, Eq, Reloc)]
#[repr(C)]
pub struct I32Data {
    pub value: i32,
}

impl CommData for I32Data {
    const ID: &'static str = "I32Data";
}

/// Test type for i64 primitive
#[derive(Debug, Clone, Copy, PartialEq, Eq, Reloc)]
#[repr(C)]
pub struct I64Data {
    pub value: i64,
}

impl CommData for I64Data {
    const ID: &'static str = "I64Data";
}

/// Test type for f32 primitive
#[derive(Debug, Clone, Copy, PartialEq, Reloc)]
#[repr(C)]
pub struct F32Data {
    pub value: f32,
}

impl CommData for F32Data {
    const ID: &'static str = "F32Data";
}

/// Test type for f64 primitive
#[derive(Debug, Clone, Copy, PartialEq, Reloc)]
#[repr(C)]
pub struct F64Data {
    pub value: f64,
}

impl CommData for F64Data {
    const ID: &'static str = "F64Data";
}

/// Test type for bool primitive
#[derive(Debug, Clone, Copy, PartialEq, Eq, Reloc)]
#[repr(C)]
pub struct BoolData {
    pub value: bool,
}

impl CommData for BoolData {
    const ID: &'static str = "BoolData";
}

/// Simple user-defined type with single field
#[derive(Debug, Clone, Copy, PartialEq, Eq, Reloc)]
#[repr(C)]
pub struct SimpleStruct {
    pub id: u32,
}

impl CommData for SimpleStruct {
    const ID: &'static str = "SimpleStruct";
}

/// User-defined type with multiple fields of different types
#[derive(Debug, Clone, Copy, PartialEq, Reloc)]
#[repr(C)]
pub struct ComplexStruct {
    pub count: u32,
    pub temperature: f32,
    pub is_active: u8,
    pub timestamp: u64,
}

impl CommData for ComplexStruct {
    const ID: &'static str = "ComplexStruct";
}

/// User-defined type with nested structure
#[derive(Debug, Clone, Copy, PartialEq, Reloc)]
#[repr(C)]
pub struct NestedStruct {
    pub id: u32,
    pub simple: SimpleStruct,
    pub value: f32,
}

impl CommData for NestedStruct {
    const ID: &'static str = "NestedStruct";
}

/// User-defined type representing a 2D point
#[derive(Debug, Clone, Copy, PartialEq, Reloc)]
#[repr(C)]
pub struct Point {
    pub x: f32,
    pub y: f32,
}

impl CommData for Point {
    const ID: &'static str = "Point";
}

/// User-defined type representing a 3D point
#[derive(Debug, Clone, Copy, PartialEq, Reloc)]
#[repr(C)]
pub struct Point3D {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

impl CommData for Point3D {
    const ID: &'static str = "Point3D";
}

/// User-defined type representing sensor data
#[derive(Debug, Clone, Copy, PartialEq, Reloc)]
#[repr(C)]
pub struct SensorData {
    pub sensor_id: u16,
    pub temperature: f32,
    pub humidity: f32,
    pub pressure: f32,
}

impl CommData for SensorData {
    const ID: &'static str = "SensorData";
}

/// User-defined type representing vehicle state
#[derive(Debug, Clone, Copy, PartialEq, Reloc)]
#[repr(C)]
pub struct VehicleState {
    pub speed: f32,
    pub rpm: u16,
    pub fuel_level: f32,
    pub is_running: u8,
    pub mileage: u32,
}

impl CommData for VehicleState {
    const ID: &'static str = "VehicleState";
}

/// Interface for testing primitive types
pub struct PrimitiveInterface {}

impl Interface for PrimitiveInterface {
    const INTERFACE_ID: &'static str = "PrimitiveInterface";
    type Consumer<R: Runtime + ?Sized> = PrimitiveConsumer<R>;
    type Producer<R: Runtime + ?Sized> = PrimitiveProducer<R>;
}

/// Consumer for primitive types interface
pub struct PrimitiveConsumer<R: Runtime + ?Sized> {
    pub u8_event: R::Subscriber<U8Data>,
    pub u16_event: R::Subscriber<U16Data>,
    pub u32_event: R::Subscriber<U32Data>,
    pub u64_event: R::Subscriber<U64Data>,
    pub i8_event: R::Subscriber<I8Data>,
    pub i16_event: R::Subscriber<I16Data>,
    pub i32_event: R::Subscriber<I32Data>,
    pub i64_event: R::Subscriber<I64Data>,
    pub f32_event: R::Subscriber<F32Data>,
    pub f64_event: R::Subscriber<F64Data>,
    pub bool_event: R::Subscriber<BoolData>,
}

impl<R: Runtime + ?Sized> Consumer<R> for PrimitiveConsumer<R> {
    fn new(instance_info: R::ConsumerInfo) -> Self {
        PrimitiveConsumer {
            u8_event: R::Subscriber::new("u8_event", instance_info.clone())
                .expect("Failed to create u8 subscriber"),
            u16_event: R::Subscriber::new("u16_event", instance_info.clone())
                .expect("Failed to create u16 subscriber"),
            u32_event: R::Subscriber::new("u32_event", instance_info.clone())
                .expect("Failed to create u32 subscriber"),
            u64_event: R::Subscriber::new("u64_event", instance_info.clone())
                .expect("Failed to create u64 subscriber"),
            i8_event: R::Subscriber::new("i8_event", instance_info.clone())
                .expect("Failed to create i8 subscriber"),
            i16_event: R::Subscriber::new("i16_event", instance_info.clone())
                .expect("Failed to create i16 subscriber"),
            i32_event: R::Subscriber::new("i32_event", instance_info.clone())
                .expect("Failed to create i32 subscriber"),
            i64_event: R::Subscriber::new("i64_event", instance_info.clone())
                .expect("Failed to create i64 subscriber"),
            f32_event: R::Subscriber::new("f32_event", instance_info.clone())
                .expect("Failed to create f32 subscriber"),
            f64_event: R::Subscriber::new("f64_event", instance_info.clone())
                .expect("Failed to create f64 subscriber"),
            bool_event: R::Subscriber::new("bool_event", instance_info.clone())
                .expect("Failed to create bool subscriber"),
        }
    }
}

/// Producer for primitive types interface
pub struct PrimitiveProducer<R: Runtime + ?Sized> {
    _runtime: core::marker::PhantomData<R>,
    instance_info: R::ProviderInfo,
}

impl<R: Runtime + ?Sized> Producer<R> for PrimitiveProducer<R> {
    type Interface = PrimitiveInterface;
    type OfferedProducer = PrimitiveOfferedProducer<R>;

    fn offer(self) -> com_api::Result<Self::OfferedProducer> {
        let offered = PrimitiveOfferedProducer {
            u8_event: R::Publisher::new("u8_event", self.instance_info.clone())
                .expect("Failed to create u8 publisher"),
            u16_event: R::Publisher::new("u16_event", self.instance_info.clone())
                .expect("Failed to create u16 publisher"),
            u32_event: R::Publisher::new("u32_event", self.instance_info.clone())
                .expect("Failed to create u32 publisher"),
            u64_event: R::Publisher::new("u64_event", self.instance_info.clone())
                .expect("Failed to create u64 publisher"),
            i8_event: R::Publisher::new("i8_event", self.instance_info.clone())
                .expect("Failed to create i8 publisher"),
            i16_event: R::Publisher::new("i16_event", self.instance_info.clone())
                .expect("Failed to create i16 publisher"),
            i32_event: R::Publisher::new("i32_event", self.instance_info.clone())
                .expect("Failed to create i32 publisher"),
            i64_event: R::Publisher::new("i64_event", self.instance_info.clone())
                .expect("Failed to create i64 publisher"),
            f32_event: R::Publisher::new("f32_event", self.instance_info.clone())
                .expect("Failed to create f32 publisher"),
            f64_event: R::Publisher::new("f64_event", self.instance_info.clone())
                .expect("Failed to create f64 publisher"),
            bool_event: R::Publisher::new("bool_event", self.instance_info.clone())
                .expect("Failed to create bool publisher"),
            instance_info: self.instance_info.clone(),
        };
        self.instance_info.offer_service()?;
        Ok(offered)
    }

    fn new(instance_info: R::ProviderInfo) -> com_api::Result<Self> {
        Ok(PrimitiveProducer {
            _runtime: core::marker::PhantomData,
            instance_info,
        })
    }
}

/// Offered producer for primitive types interface
pub struct PrimitiveOfferedProducer<R: Runtime + ?Sized> {
    pub u8_event: R::Publisher<U8Data>,
    pub u16_event: R::Publisher<U16Data>,
    pub u32_event: R::Publisher<U32Data>,
    pub u64_event: R::Publisher<U64Data>,
    pub i8_event: R::Publisher<I8Data>,
    pub i16_event: R::Publisher<I16Data>,
    pub i32_event: R::Publisher<I32Data>,
    pub i64_event: R::Publisher<I64Data>,
    pub f32_event: R::Publisher<F32Data>,
    pub f64_event: R::Publisher<F64Data>,
    pub bool_event: R::Publisher<BoolData>,
    instance_info: R::ProviderInfo,
}

impl<R: Runtime + ?Sized> OfferedProducer<R> for PrimitiveOfferedProducer<R> {
    type Interface = PrimitiveInterface;
    type Producer = PrimitiveProducer<R>;

    fn unoffer(self) -> com_api::Result<Self::Producer> {
        let producer = PrimitiveProducer {
            _runtime: core::marker::PhantomData,
            instance_info: self.instance_info.clone(),
        };
        self.instance_info.stop_offer_service()?;
        Ok(producer)
    }
}

/// Interface for testing user-defined types
pub struct UserDefinedInterface {}

impl Interface for UserDefinedInterface {
    const INTERFACE_ID: &'static str = "UserDefinedInterface";
    type Consumer<R: Runtime + ?Sized> = UserDefinedConsumer<R>;
    type Producer<R: Runtime + ?Sized> = UserDefinedProducer<R>;
}

/// Consumer for user-defined types interface
pub struct UserDefinedConsumer<R: Runtime + ?Sized> {
    pub simple_event: R::Subscriber<SimpleStruct>,
    pub complex_event: R::Subscriber<ComplexStruct>,
    pub nested_event: R::Subscriber<NestedStruct>,
    pub point_event: R::Subscriber<Point>,
    pub point3d_event: R::Subscriber<Point3D>,
    pub sensor_event: R::Subscriber<SensorData>,
    pub vehicle_event: R::Subscriber<VehicleState>,
}

impl<R: Runtime + ?Sized> Consumer<R> for UserDefinedConsumer<R> {
    fn new(instance_info: R::ConsumerInfo) -> Self {
        UserDefinedConsumer {
            simple_event: R::Subscriber::new("simple_event", instance_info.clone())
                .expect("Failed to create simple subscriber"),
            complex_event: R::Subscriber::new("complex_event", instance_info.clone())
                .expect("Failed to create complex subscriber"),
            nested_event: R::Subscriber::new("nested_event", instance_info.clone())
                .expect("Failed to create nested subscriber"),
            point_event: R::Subscriber::new("point_event", instance_info.clone())
                .expect("Failed to create point subscriber"),
            point3d_event: R::Subscriber::new("point3d_event", instance_info.clone())
                .expect("Failed to create point3d subscriber"),
            sensor_event: R::Subscriber::new("sensor_event", instance_info.clone())
                .expect("Failed to create sensor subscriber"),
            vehicle_event: R::Subscriber::new("vehicle_event", instance_info.clone())
                .expect("Failed to create vehicle subscriber"),
        }
    }
}

/// Producer for user-defined types interface
pub struct UserDefinedProducer<R: Runtime + ?Sized> {
    _runtime: core::marker::PhantomData<R>,
    instance_info: R::ProviderInfo,
}

impl<R: Runtime + ?Sized> Producer<R> for UserDefinedProducer<R> {
    type Interface = UserDefinedInterface;
    type OfferedProducer = UserDefinedOfferedProducer<R>;

    fn offer(self) -> com_api::Result<Self::OfferedProducer> {
        let offered = UserDefinedOfferedProducer {
            simple_event: R::Publisher::new("simple_event", self.instance_info.clone())
                .expect("Failed to create simple publisher"),
            complex_event: R::Publisher::new("complex_event", self.instance_info.clone())
                .expect("Failed to create complex publisher"),
            nested_event: R::Publisher::new("nested_event", self.instance_info.clone())
                .expect("Failed to create nested publisher"),
            point_event: R::Publisher::new("point_event", self.instance_info.clone())
                .expect("Failed to create point publisher"),
            point3d_event: R::Publisher::new("point3d_event", self.instance_info.clone())
                .expect("Failed to create point3d publisher"),
            sensor_event: R::Publisher::new("sensor_event", self.instance_info.clone())
                .expect("Failed to create sensor publisher"),
            vehicle_event: R::Publisher::new("vehicle_event", self.instance_info.clone())
                .expect("Failed to create vehicle publisher"),
            instance_info: self.instance_info.clone(),
        };
        self.instance_info.offer_service()?;
        Ok(offered)
    }

    fn new(instance_info: R::ProviderInfo) -> com_api::Result<Self> {
        Ok(UserDefinedProducer {
            _runtime: core::marker::PhantomData,
            instance_info,
        })
    }
}

/// Offered producer for user-defined types interface
pub struct UserDefinedOfferedProducer<R: Runtime + ?Sized> {
    pub simple_event: R::Publisher<SimpleStruct>,
    pub complex_event: R::Publisher<ComplexStruct>,
    pub nested_event: R::Publisher<NestedStruct>,
    pub point_event: R::Publisher<Point>,
    pub point3d_event: R::Publisher<Point3D>,
    pub sensor_event: R::Publisher<SensorData>,
    pub vehicle_event: R::Publisher<VehicleState>,
    instance_info: R::ProviderInfo,
}

impl<R: Runtime + ?Sized> OfferedProducer<R> for UserDefinedOfferedProducer<R> {
    type Interface = UserDefinedInterface;
    type Producer = UserDefinedProducer<R>;

    fn unoffer(self) -> com_api::Result<Self::Producer> {
        let producer = UserDefinedProducer {
            _runtime: core::marker::PhantomData,
            instance_info: self.instance_info.clone(),
        };
        self.instance_info.stop_offer_service()?;
        Ok(producer)
    }
}
