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
#ifndef SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_I_TRACING_FILTER_CONFIG_H
#define SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_I_TRACING_FILTER_CONFIG_H

#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/tracing/configuration/proxy_event_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/proxy_field_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/skeleton_event_trace_point_type.h"
#include "score/mw/com/impl/tracing/configuration/skeleton_field_trace_point_type.h"

#include <string_view>

namespace score::mw::com::impl::tracing
{

class ITracingFilterConfig
{
  public:
    using InstanceSpecifierView = std::string_view;

    ITracingFilterConfig() noexcept = default;
    virtual ~ITracingFilterConfig() = default;

  protected:
    ITracingFilterConfig(const ITracingFilterConfig&) = default;
    ITracingFilterConfig(ITracingFilterConfig&&) noexcept = default;
    ITracingFilterConfig& operator=(const ITracingFilterConfig&) = default;
    ITracingFilterConfig& operator=(ITracingFilterConfig&&) noexcept = default;

  public:
    virtual bool IsTracePointEnabled(std::string_view service_type,
                                     std::string_view event_name,
                                     InstanceSpecifierView instance_specifier,
                                     SkeletonEventTracePointType skeleton_event_trace_point_type) const noexcept = 0;
    virtual bool IsTracePointEnabled(std::string_view service_type,
                                     std::string_view field_name,
                                     InstanceSpecifierView instance_specifier,
                                     SkeletonFieldTracePointType skeleton_field_trace_point_type) const noexcept = 0;
    virtual bool IsTracePointEnabled(std::string_view service_type,
                                     std::string_view event_name,
                                     InstanceSpecifierView instance_specifier,
                                     ProxyEventTracePointType proxy_event_trace_point_type) const noexcept = 0;
    virtual bool IsTracePointEnabled(std::string_view service_type,
                                     std::string_view field_name,
                                     InstanceSpecifierView instance_specifier,
                                     ProxyFieldTracePointType proxy_field_trace_point_type) const noexcept = 0;

    virtual void AddTracePoint(std::string_view service_type,
                               std::string_view event_name,
                               InstanceSpecifierView instance_specifier,
                               SkeletonEventTracePointType skeleton_event_trace_point_type) noexcept = 0;
    virtual void AddTracePoint(std::string_view service_type,
                               std::string_view field_name,
                               InstanceSpecifierView instance_specifier,
                               SkeletonFieldTracePointType skeleton_field_trace_point_type) noexcept = 0;
    virtual void AddTracePoint(std::string_view service_type,
                               std::string_view event_name,
                               InstanceSpecifierView instance_specifier,
                               ProxyEventTracePointType proxy_event_trace_point_type) noexcept = 0;
    virtual void AddTracePoint(std::string_view service_type,
                               std::string_view field_name,
                               InstanceSpecifierView instance_specifier,
                               ProxyFieldTracePointType proxy_field_trace_point_type) noexcept = 0;

    virtual std::uint16_t GetNumberOfTracingSlots(score::mw::com::impl::Configuration& config) const noexcept = 0;
};

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_I_TRACING_FILTER_CONFIG_H
