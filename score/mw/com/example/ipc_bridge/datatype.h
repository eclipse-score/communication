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
#ifndef SCORE_IPC_BRIDGE_DATATYPE_H
#define SCORE_IPC_BRIDGE_DATATYPE_H

#include "score/mw/com/types.h"

namespace score::mw::com
{

constexpr std::size_t MAX_SUCCESSORS = 16U;
constexpr std::size_t MAX_LANES = 16U;

enum class StdTimestampSyncState : std::uint32_t
{
    /// @brief Timestamp is in sync with the global master, MAX_DIFF property is guaranteed.
    kStdTimestampSyncState_InSync = 0U,
    /// @brief Timestamp is not in sync with the global master, no property guarantees can be given, use at your own
    /// risk.
    kStdTimestampSyncState_NotInSync = 1U,
    /// @brief No timestamp is available due to infrastructure reasons (e.g. initial value, or no StbM integrated, or
    /// prediction target timestamp cannot be calculated, ...).
    kStdTimestampSyncState_Invalid = 255U
};

struct StdTimestamp
{
    /// @brief The sub-seconds part of the timestamp
    ///
    //// unit: [ns]
    std::uint32_t fractional_seconds;
    /// @brief The seconds part of the timestamp.
    ///
    /// unit: [s]
    std::uint32_t seconds;
    /// @brief Status whether the timestamp is in sync with the global master or not.
    StdTimestampSyncState sync_status;
};

enum class EventDataQualifier : std::uint32_t
{
    /// @brief Event data available, normal operation.
    ///
    /// The event is valid and all data elements in the scope of the qualifier should be evaluated. Parts of the service
    /// may still be in degradation (i.e.contained qualifiers or quality of service attributes (e.g. standard deviation)
    /// must be evaluated).
    kEventDataQualifier_EventDataAvailable = 0U,
    /// @brief Event data available, but a degradation condition applies (e.g. calibration). The reason of the
    /// degradation is stored in the parameter extendedQualifier.
    ///
    /// Parts of the data may still be in degradation. Therefore, the receiver must decide (based on contained
    /// qualifiers or quality of service attributes) whether the data can be still used.
    kEventDataQualifier_EventDataAvailableReduced = 1U,
    /// @brief Data for this event is currently not available. The extendedQualifier (if present) contains information
    /// on the reason for non-availability.
    ///
    /// The remaining information in the scope of the event (except extendedQualifier) must not be evaluated.
    kEventDataQualifier_EventDataNotAvailable = 2U,
    /// @brief Data for this event is currently unknown.
    ///
    /// The remaining information in the scope of the event (except extendedQualifier) must not be evaluated.
    // kEventDataQualifier_EventDataUnknown = 14U,
    /// @brief There is no event data available, due to the event data being invalid (e.g. CRC error) or due to a
    /// timeout.
    ///
    /// The remaining information in the scope of the event (except extendedQualifier) must not be evaluated.
    kEventDataQualifier_EventDataInvalid = 255U
};

struct MapApiLaneBoundaryData
{
};

using LaneIdType = std::size_t;
using LaneWidth = std::size_t;
using LaneBoundaryId = std::size_t;

namespace map_api
{

using LinkId = std::size_t;
using LengthM = std::double_t;

struct LaneConnectionInfo
{
};

using LaneConnectionInfoList = std::array<LaneConnectionInfo, 10U>;

struct LaneRestrictionInfo
{
};

using LaneRestrictionInfoList = std::array<LaneRestrictionInfo, 10U>;

struct ShoulderLaneInfo
{
};

using ShoulderLaneInfoList = std::array<ShoulderLaneInfo, 10U>;

struct LaneToLinkAssociation
{
};

using LaneUsedInBothDirections = bool;

}  // namespace map_api

namespace adp
{

struct MapApiPointData
{
};

enum class LaneType : std::size_t
{
    UNKNOWN,
};

enum class LaneTypeNew : std::size_t
{
    Unknown,
};

struct TurnDirection
{
};

namespace map_api
{

using SpeedLimit = std::size_t;
using LaneFollowsMpp = bool;

}  // namespace map_api

}  // namespace adp

struct MapApiLaneData
{
    MapApiLaneData() = default;

    MapApiLaneData(MapApiLaneData&&) = default;

    MapApiLaneData(const MapApiLaneData&) = default;

    MapApiLaneData& operator=(MapApiLaneData&&) = default;

    MapApiLaneData& operator=(const MapApiLaneData&) = default;

    /// @brief range: [1, n]. Unique ID of the lane
    LaneIdType lane_id{0U};

    /// @brief range: [1, n]. The IDs of all links that this lane belongs to
    std::array<map_api::LinkId, 10U> link_ids;

    /// @brief The IDs of all lane from which this lane can be reached in longitudinal direction
    std::array<LaneIdType, 10U> predecessor_lanes;

    /// @brief The IDs of all lane that can be reached from this lane in longitudinal direction
    std::array<LaneIdType, MAX_SUCCESSORS> successor_lanes;

    /// @brief The center line of this lane
    std::array<adp::MapApiPointData, 10U> center_line;

    /// @brief The innermost left boundary at the beginning of this lane
    LaneBoundaryId left_boundary_id{0U};

    /// @brief The innermost right boundary at the beginning of this lane
    LaneBoundaryId right_boundary_id{0U};

    /// @brief The ID of the lane to the left
    /// @note 0 indicates that there is no lane to the left
    LaneIdType left_lane_id{0U};

    /// @brief The id of the lane ro the right
    /// @note 0 indicates that there is no lane to the right
    LaneIdType right_lane_id{0U};

    /// @brief The type of the lane
    adp::LaneType lane_type{adp::LaneType::UNKNOWN};

    /// @brief The type of the lane
    adp::LaneTypeNew lane_type_new{adp::LaneTypeNew::Unknown};

    /// @brief Describes Lane Connection Type and the range on the lane for which it applies
    map_api::LaneConnectionInfoList lane_connection_info;

    /// @brief Describes Lane Restriction Type and the range on the lane for which it applies
    map_api::LaneRestrictionInfoList lane_restriction_info;

    /// @brief Describes Shoulder Lane Type and the range on the lane for which it applies
    /// @note not provided by MapDAL as of 6.08.2021r
    map_api::ShoulderLaneInfoList shoulder_lane_info;

    /// @brief The turn direction associated with the lane
    adp::TurnDirection turn_direction;
    /// @brief unit: [cm]. The width of the current lane
    /// @details This is the smallest width over the whole lane. When the lane is splitting or
    ///          merging, the width can be 0.
    ///          The width is also set to 0 when no width is available.
    LaneWidth width_in_cm{0U};

    /// @brief unit: [m]. The length of the current lane
    map_api::LengthM length_in_m{0.0};

    /// @brief The speed limits on the current lane
    std::array<adp::map_api::SpeedLimit, 10U> speed_limits;

    /// @brief struct describing whether the lane is part of calculated Most Probable Path, or if yes within a range
    adp::map_api::LaneFollowsMpp lane_follows_mpp;

    /// @brief Boolean flag describing whether lane is fully attributed
    bool is_fully_attributed{false};

    /// @brief array containing the IDs of all left lane boundaries ordered from curb to middle
    std::array<LaneBoundaryId, 10U> left_lane_boundaries_ids;

    /// @brief array containing the IDs of all right lane boundaries ordered from curb to middle
    std::array<LaneBoundaryId, 10U> right_lane_boundaries_ids;

    /// @brief links associated with current lane
    std::array<map_api::LaneToLinkAssociation, 10U> link_associations;

    /// @brief array of lane ranges where lane can be used in both directions.
    std::array<map_api::LaneUsedInBothDirections, 10U> used_in_both_directions;
};

struct LaneGroupData
{
};

struct MapApiLanesStamped
{
    MapApiLanesStamped() = default;

    MapApiLanesStamped(MapApiLanesStamped&&) = default;

    MapApiLanesStamped(const MapApiLanesStamped&) = default;

    MapApiLanesStamped& operator=(MapApiLanesStamped&&) = default;

    MapApiLanesStamped& operator=(const MapApiLanesStamped&) = default;

    StdTimestamp time_stamp{0, 0, StdTimestampSyncState::kStdTimestampSyncState_Invalid};

    /// @brief A name of the coordinate frame, used while fetching data.
    ///
    /// Depending on the driving scenario, different coordinate frames can be used.
    /// Case "map_debug" : for Highway scenario it is an NTM planar coordinate system.
    /// Case: "local_map_frame": for Urban scenario it is a vehicle's local coordinate system.
    std::array<char, 10U> frame_id;

    /// @brief Current projection id.
    ///
    /// In case of NTM geodetic reference system, a zone can be of an arbitrary size, thus doesn't have a fixed
    /// descriptor. This variable provides an index of the zone, in which the vehicle is currently located.
    ///
    /// range: [0, n]
    std::uint32_t projection_id{};

    /// @brief Describes the different kinds of quality levels of interface data. (placeholder for future concrete
    /// implementation, for now we just initialize by not available)
    EventDataQualifier event_data_qualifier{EventDataQualifier::kEventDataQualifier_EventDataNotAvailable};

    /// @brief An array, containing lane boundaries, which refer to lanes from the given parent data structure. Lane
    /// boundary indicates edge of the lane.
    std::array<MapApiLaneBoundaryData, 10U> lane_boundaries;

    /// @brief All lanes from HD map for a relevant piece of road.
    std::array<MapApiLaneData, MAX_LANES> lanes;

    std::array<LaneGroupData, 10U> lane_groups;

    std::uint32_t x;
    std::size_t hash_value;
};

template <typename Trait>
class IpcBridgeInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Event<MapApiLanesStamped> map_api_lanes_stamped_{*this, "map_api_lanes_stamped"};
};

using IpcBridgeProxy = AsProxy<IpcBridgeInterface>;
using IpcBridgeSkeleton = AsSkeleton<IpcBridgeInterface>;

}  // namespace score::mw::com

#endif  // SCORE_IPC_BRIDGE_DATATYPE_H
