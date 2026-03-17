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

//! Generated bindings for the BigData interface using the com-api library.
//!
//! This file is the com-api counterpart of the old-API `bigdata_gen.rs`.  It
//! defines the same large data types (`MapApiLanesStamped`, `DummyDataStamped`)
//! and wires them through the com-api `Interface` / `Consumer` / `Producer`
//! abstractions instead of the legacy `mw_com::import_interface!` macro.

use com_api::{
    CommData, Consumer, Interface, OfferedProducer, Producer, ProviderInfo, Publisher, Reloc,
    Runtime, Subscriber,
};

use core::fmt::Debug;

#[repr(C)]
#[derive(Default, Reloc)]
pub struct StdTimestamp {
    pub fractional_seconds: u32,
    pub seconds: u32,
    pub sync_status: u32,
}

/// Opaque representation of `MapApiLaneBoundaryData` (empty struct in C++).
#[repr(C)]
#[derive(Default, Reloc)]
pub struct MapApiLaneBoundaryData {
    _dummy: [u8; 1],
}

/// Opaque representation of `MapApiLaneData`.
/// Using a single-byte placeholder; the C++ type is much larger.
/// This is sufficient because the test only verifies sample-count delivery.
#[repr(C)]
#[derive(Default, Reloc)]
pub struct MapApiLaneData {
    _dummy: [u8; 1],
}

/// Opaque representation of `LaneGroupData` (empty struct in C++).
#[repr(C)]
#[derive(Default, Reloc)]
pub struct LaneGroupData {
    _dummy: [u8; 1],
}

pub const MAX_LANES: usize = 16;

/// Large data type sent over the `map_api_lanes_stamped` event.
#[repr(C)]
#[derive(Default, Reloc)]
pub struct MapApiLanesStamped {
    pub time_stamp: StdTimestamp,
    pub frame_id: [u8; 10],
    pub projection_id: u32,
    pub event_data_qualifier: u32,
    pub lane_boundaries: [MapApiLaneBoundaryData; 10],
    pub lanes: [MapApiLaneData; MAX_LANES],
    pub lane_groups: [LaneGroupData; 10],
    pub x: u32,
    pub hash_value: usize,
}

impl CommData for MapApiLanesStamped {
    const ID: &'static str = "MapApiLanesStamped";
}

impl Debug for MapApiLanesStamped {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "MapApiLanesStamped {{ x: {} }}", self.x)
    }
}

/// Secondary event data type.
#[repr(C)]
#[derive(Default, Reloc)]
pub struct DummyDataStamped {
    pub time_stamp: StdTimestamp,
    pub x: u8,
    pub hash_value: usize,
}

impl CommData for DummyDataStamped {
    const ID: &'static str = "DummyDataStamped";
}

impl Debug for DummyDataStamped {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "DummyDataStamped {{ x: {} }}", self.x)
    }
}

/// Generated com-api bindings for the BigData interface.
pub struct BigDataInterface {}

impl Interface for BigDataInterface {
    const INTERFACE_ID: &'static str = "BigDataInterface";
    type Consumer<R: Runtime + ?Sized> = BigDataConsumer<R>;
    type Producer<R: Runtime + ?Sized> = BigDataProducer<R>;
}

/// Consumer type for the BigData interface.
pub struct BigDataConsumer<R: Runtime + ?Sized> {
    pub map_api_lanes_stamped: R::Subscriber<MapApiLanesStamped>,
    pub dummy_data_stamped: R::Subscriber<DummyDataStamped>,
}

impl<R: Runtime + ?Sized> Consumer<R> for BigDataConsumer<R> {
    fn new(instance_info: R::ConsumerInfo) -> Self {
        let map_api_lanes_stamped =
            R::Subscriber::new("map_api_lanes_stamped_", instance_info.clone())
                .expect("Failed to create map_api_lanes_stamped subscriber");
        let dummy_data_stamped = R::Subscriber::new("dummy_data_stamped_", instance_info)
            .expect("Failed to create dummy_data_stamped subscriber");
        BigDataConsumer {
            map_api_lanes_stamped,
            dummy_data_stamped,
        }
    }
}

/// Producer type for the BigData interface.
pub struct BigDataProducer<R: Runtime + ?Sized> {
    _runtime: core::marker::PhantomData<R>,
    instance_info: R::ProviderInfo,
}

impl<R: Runtime + ?Sized> Producer<R> for BigDataProducer<R> {
    type Interface = BigDataInterface;
    type OfferedProducer = BigDataOfferedProducer<R>;

    fn new(instance_info: R::ProviderInfo) -> com_api::Result<Self> {
        Ok(BigDataProducer {
            _runtime: core::marker::PhantomData,
            instance_info,
        })
    }

    fn offer(self) -> com_api::Result<Self::OfferedProducer> {
        let map_api_lanes_stamped =
            R::Publisher::new("map_api_lanes_stamped_", self.instance_info.clone())
                .expect("Failed to create map_api_lanes_stamped publisher");
        let dummy_data_stamped =
            R::Publisher::new("dummy_data_stamped_", self.instance_info.clone())
                .expect("Failed to create dummy_data_stamped publisher");
        self.instance_info.offer_service()?;
        Ok(BigDataOfferedProducer {
            map_api_lanes_stamped,
            dummy_data_stamped,
            instance_info: self.instance_info,
        })
    }
}

/// Offered producer type for the BigData interface, returned by `BigDataProducer::offer()`.
pub struct BigDataOfferedProducer<R: Runtime + ?Sized> {
    pub map_api_lanes_stamped: R::Publisher<MapApiLanesStamped>,
    pub dummy_data_stamped: R::Publisher<DummyDataStamped>,
    instance_info: R::ProviderInfo,
}

impl<R: Runtime + ?Sized> OfferedProducer<R> for BigDataOfferedProducer<R> {
    type Interface = BigDataInterface;
    type Producer = BigDataProducer<R>;

    fn unoffer(self) -> com_api::Result<Self::Producer> {
        self.instance_info.stop_offer_service()?;
        Ok(BigDataProducer {
            _runtime: core::marker::PhantomData,
            instance_info: self.instance_info,
        })
    }
}
