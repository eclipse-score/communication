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
#ifndef SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACING_FILTER_CONFIG_MOCK_H
#define SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACING_FILTER_CONFIG_MOCK_H

#include "score/mw/com/impl/tracing/configuration/i_tracing_filter_config.h"

#include <gmock/gmock.h>

namespace score::mw::com::impl::tracing
{

class TracingFilterConfigMock : public ITracingFilterConfig
{
  public:
    MOCK_METHOD(bool,
                IsTracePointEnabled,
                (std::string_view service_type,
                 std::string_view event_name,
                 InstanceSpecifierView instance_specifier,
                 SkeletonEventTracePointType skeleton_event_trace_point_type),
                (const, noexcept, override));
    MOCK_METHOD(bool,
                IsTracePointEnabled,
                (std::string_view service_type,
                 std::string_view event_name,
                 InstanceSpecifierView instance_specifier,
                 SkeletonFieldTracePointType skeleton_field_trace_point_type),
                (const, noexcept, override));
    MOCK_METHOD(bool,
                IsTracePointEnabled,
                (std::string_view service_type,
                 std::string_view event_name,
                 InstanceSpecifierView instance_specifier,
                 ProxyEventTracePointType proxy_event_trace_point_type),
                (const, noexcept, override));
    MOCK_METHOD(bool,
                IsTracePointEnabled,
                (std::string_view service_type,
                 std::string_view event_name,
                 InstanceSpecifierView instance_specifier,
                 ProxyFieldTracePointType proxy_field_trace_point_type),
                (const, noexcept, override));

    MOCK_METHOD(void,
                AddTracePoint,
                (std::string_view service_type,
                 std::string_view event_name,
                 InstanceSpecifierView instance_specifier,
                 SkeletonEventTracePointType skeleton_event_trace_point_type),
                (noexcept, override));
    MOCK_METHOD(void,
                AddTracePoint,
                (std::string_view service_type,
                 std::string_view event_name,
                 InstanceSpecifierView instance_specifier,
                 SkeletonFieldTracePointType skeleton_field_trace_point_type),
                (noexcept, override));
    MOCK_METHOD(void,
                AddTracePoint,
                (std::string_view service_type,
                 std::string_view event_name,
                 InstanceSpecifierView instance_specifier,
                 ProxyEventTracePointType proxy_event_trace_point_type),
                (noexcept, override));
    MOCK_METHOD(void,
                AddTracePoint,
                (std::string_view service_type,
                 std::string_view event_name,
                 InstanceSpecifierView instance_specifier,
                 ProxyFieldTracePointType proxy_field_trace_point_type),
                (noexcept, override));
    MOCK_METHOD(std::uint16_t,
                GetNumberOfTracingSlots,
                (score::mw::com::impl::Configuration & config),
                (const, noexcept, override));
};

}  // namespace score::mw::com::impl::tracing

#endif  // SCORE_MW_COM_IMPL_TRACING_CONFIGURATION_TRACING_FILTER_CONFIG_MOCK_H
