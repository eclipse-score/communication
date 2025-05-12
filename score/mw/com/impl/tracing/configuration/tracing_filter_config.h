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
#ifndef SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACING_FILTER_CONFIG_H
#define SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACING_FILTER_CONFIG_H

#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/tracing/configuration/i_tracing_filter_config.h"
#include "score/mw/com/impl/tracing/configuration/trace_point_key.h"

#include <score/string_view.hpp>

#include <set>
#include <string>
#include <unordered_map>

namespace score::mw::com::impl::tracing
{

namespace detail_tracing_filter_config
{

class CompareStringWithStringView
{
  public:
    using is_transparent = void;
    bool operator()(const std::string& lhs, const std::string& rhs) const noexcept
    {
        return lhs < rhs;
    }
    bool operator()(const score::cpp::string_view lhs_view, const std::string& rhs_string) const noexcept;
    bool operator()(const std::string& lhs_string, const score::cpp::string_view rhs_view) const noexcept;
};

}  // namespace detail_tracing_filter_config

class TracingFilterConfig : public ITracingFilterConfig
{
  public:
    bool IsTracePointEnabled(score::cpp::string_view service_type,
                             score::cpp::string_view event_name,
                             InstanceSpecifierView instance_specifier,
                             SkeletonEventTracePointType skeleton_event_trace_point_type) const noexcept override;
    bool IsTracePointEnabled(score::cpp::string_view service_type,
                             score::cpp::string_view field_name,
                             InstanceSpecifierView instance_specifier,
                             SkeletonFieldTracePointType skeleton_field_trace_point_type) const noexcept override;
    bool IsTracePointEnabled(score::cpp::string_view service_type,
                             score::cpp::string_view event_name,
                             InstanceSpecifierView instance_specifier,
                             ProxyEventTracePointType proxy_event_trace_point_type) const noexcept override;
    bool IsTracePointEnabled(score::cpp::string_view service_type,
                             score::cpp::string_view field_name,
                             InstanceSpecifierView instance_specifier,
                             ProxyFieldTracePointType proxy_field_trace_point_type) const noexcept override;

    void AddTracePoint(score::cpp::string_view service_type,
                       score::cpp::string_view event_name,
                       InstanceSpecifierView instance_specifier,
                       SkeletonEventTracePointType skeleton_event_trace_point_type) noexcept override;
    void AddTracePoint(score::cpp::string_view service_type,
                       score::cpp::string_view field_name,
                       InstanceSpecifierView instance_specifier,
                       SkeletonFieldTracePointType skeleton_field_trace_point_type) noexcept override;
    void AddTracePoint(score::cpp::string_view service_type,
                       score::cpp::string_view event_name,
                       InstanceSpecifierView instance_specifier,
                       ProxyEventTracePointType proxy_event_trace_point_type) noexcept override;
    void AddTracePoint(score::cpp::string_view service_type,
                       score::cpp::string_view field_name,
                       InstanceSpecifierView instance_specifier,
                       ProxyFieldTracePointType proxy_field_trace_point_type) noexcept override;

    std::uint16_t GetNumberOfTracingSlots(score::mw::com::impl::Configuration& config) const noexcept override;

  private:
    std::set<std::string, detail_tracing_filter_config::CompareStringWithStringView> config_names_;

    using TracePointMapType = std::unordered_map<TracePointKey, std::set<InstanceSpecifierView>>;
    TracePointMapType skeleton_event_trace_points_;
    TracePointMapType skeleton_field_trace_points_;
    TracePointMapType proxy_event_trace_points_;
    TracePointMapType proxy_field_trace_points_;
};

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACING_FILTER_CONFIG_H
