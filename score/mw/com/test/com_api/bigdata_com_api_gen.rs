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

use com_api::{interface, CommData, ProviderInfo, Publisher, Reloc, Subscriber};

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

interface!(
    interface BigData, {
        Id = "BigDataInterface",
        map_api_lanes_stamped_: Event<MapApiLanesStamped>,
        dummy_data_stamped_: Event<DummyDataStamped>,
     }
);
