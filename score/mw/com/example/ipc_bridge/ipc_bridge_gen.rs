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
///! This is the "generated" part for the ipc_bridge proxy. Its main purpose is to provide the imports
///! of the type- and name-dependent part of the FFI and create the respective user-facing objects.
use std::default::Default;
use std::ffi::c_char;

pub const MAX_SUCCESSORS: libc::size_t = 16;
pub const MAX_LANES: libc::size_t = 16;

#[repr(u32)]
pub enum StdTimestampSyncState {
    #[allow(non_camel_case_types)]
    kStdTimestampSyncState_InSync = 0,
    #[allow(non_camel_case_types)]
    kStdTimestampSyncState_NotInSync = 1,
    #[allow(non_camel_case_types)]
    kStdTimestampSyncState_Invalid = 255,
}

impl Default for StdTimestampSyncState {
    fn default() -> Self {
        StdTimestampSyncState::kStdTimestampSyncState_Invalid
    }
}

#[repr(C)]
#[derive(Default)]
pub struct StdTimestamp {
    fractional_seconds: u32,
    seconds: u32,
    sync_status: StdTimestampSyncState,
}

#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EventDataQualifier {
    /// @brief Event data available, normal operation.
    ///
    /// The event is valid and all data elements in the scope of the qualifier should be evaluated. Parts of the service
    /// may still be in degradation (i.e.contained qualifiers or quality of service attributes (e.g. standard deviation)
    /// must be evaluated).
    EventDataAvailable = 0,
    /// @brief Event data available, but a degradation condition applies (e.g. calibration). The reason of the
    /// degradation is stored in the parameter extendedQualifier.
    ///
    /// Parts of the data may still be in degradation. Therefore, the receiver must decide (based on contained
    /// qualifiers or quality of service attributes) whether the data can be still used.
    EventDataAvailableReduced = 1,
    /// @brief Data for this event is currently not available. The extendedQualifier (if present) contains information
    /// on the reason for non-availability.
    ///
    /// The remaining information in the scope of the event (except extendedQualifier) must not be evaluated.
    EventDataNotAvailable = 2,
    /// @brief There is no event data available, due to the event data being invalid (e.g. CRC error) or due to a
    /// timeout.
    ///
    /// The remaining information in the scope of the event (except extendedQualifier) must not be evaluated.
    EventDataInvalid = 255,
}

impl Default for EventDataQualifier {
    fn default() -> Self {
        EventDataQualifier::EventDataNotAvailable
    }
}

#[repr(C)]
#[derive(Default)]
pub struct MapApiLaneBoundaryData {
    _dummy: [u8; 1],
}

type LaneIdType = libc::size_t;
type LaneWidth = libc::size_t;
type LaneBoundaryId = libc::size_t;

mod map_api {
    pub type LinkId = libc::size_t;
    pub type LengthM = libc::c_double;

    #[repr(C)]
    #[derive(Default)]
    pub struct LaneConnectionInfo {
        _dummy: [u8; 1],
    }

    pub type LaneConnectionInfoList = [LaneConnectionInfo; 10];

    #[repr(C)]
    #[derive(Default)]
    pub struct LaneRestrictionInfo {
        _dummy: [u8; 1],
    }

    pub type LaneRestrictionInfoList = [LaneRestrictionInfo; 10];

    #[repr(C)]
    #[derive(Default)]
    pub struct ShoulderLaneInfo {
        _dummy: [u8; 1],
    }

    pub type ShoulderLaneInfoList = [ShoulderLaneInfo; 10];

    #[repr(C)]
    #[derive(Default)]
    pub struct LaneToLinkAssociation {
        _dummy: [u8; 1],
    }

    pub type LaneUsedInBothDirections = bool;
}

pub mod adp {
    #[repr(C)]
    #[derive(Default)]
    pub struct MapApiPointData {
        _dummy: [u8; 1],
    }

    #[repr(C)]
    #[derive(Default)]
    pub struct TurnDirection {
        _dummy: [u8; 1],
    }

    pub type LaneType = libc::size_t;
    pub type LaneTypeNew = libc::size_t;

    pub mod map_api {
        pub type SpeedLimit = libc::size_t;
        pub type LaneFollowsMpp = bool;
    }
}

#[repr(C)]
#[derive(Default)]
pub struct MapApiLaneData {
    lane_id: LaneIdType,
    link_ids: [map_api::LinkId; 10],
    predecessor_lanes: [LaneIdType; 10],
    successor_lanes: [LaneIdType; MAX_SUCCESSORS],
    center_line: [adp::MapApiPointData; 10],
    left_boundary_id: LaneBoundaryId,
    right_boundary_id: LaneBoundaryId,
    left_lane_id: LaneIdType,
    right_lane_id: LaneIdType,
    lane_type: adp::LaneType,
    lane_type_new: adp::LaneTypeNew,
    lane_connection_info: map_api::LaneConnectionInfoList,
    lane_restriction_info: map_api::LaneRestrictionInfoList,
    shoulder_lane_info: map_api::ShoulderLaneInfoList,
    turn_direction: adp::TurnDirection,
    width_in_cm: LaneWidth,
    length_in_m: map_api::LengthM,
    speed_limits: [adp::map_api::SpeedLimit; 10],
    lane_follows_mpp: adp::map_api::LaneFollowsMpp,
    is_fully_attributed: bool,
    left_lane_boundaries_ids: [LaneBoundaryId; 10],
    right_lane_boundaries_ids: [LaneBoundaryId; 10],
    link_associations: [map_api::LaneToLinkAssociation; 10],
    used_in_both_directions: [map_api::LaneUsedInBothDirections; 10],
}

#[repr(C)]
#[derive(Default)]
pub struct LaneGroupData {
    _dummy: [u8; 1],
}

#[repr(C)]
#[derive(Default)]
pub struct MapApiLanesStamped {
    time_stamp: StdTimestamp,
    frame_id: [c_char; 10],
    projection_id: u32,
    event_data_qualifier: EventDataQualifier,
    pub lane_boundaries: [MapApiLaneBoundaryData; 10],
    lanes: [MapApiLaneData; MAX_LANES],
    lane_groups: [LaneGroupData; 10],
    pub x: u32,
    hash_value: libc::size_t,
}

mw_com::import_interface!(mw_com_IpcBridge, IpcBridge, {
    map_api_lanes_stamped_: Event<crate::MapApiLanesStamped>
});

mw_com::import_type!(mw_com_MapApiLanesStamped, crate::MapApiLanesStamped);
