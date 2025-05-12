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
#include "score/mw/com/impl/tracing/common_event_tracing.h"

#include "score/mw/com/impl/runtime.h"

#include "score/result/result.h"

#include <score/assert.hpp>

#include <utility>

namespace score::mw::com::impl::tracing
{

namespace
{

std::string_view GetServiceType(const InstanceIdentifier& instance_identifier) noexcept
{
    InstanceIdentifierView instance_identifier_view{instance_identifier};
    const auto& service_instance_deployment = instance_identifier_view.GetServiceInstanceDeployment();
    return service_instance_deployment.service_.ToString();
}

std::string_view GetInstanceSpecifier(const InstanceIdentifier& instance_identifier) noexcept
{
    InstanceIdentifierView instance_identifier_view{instance_identifier};
    const auto& service_instance_deployment = instance_identifier_view.GetServiceInstanceDeployment();
    return service_instance_deployment.instance_specifier_.ToString();
}

}  // namespace

ResultBlank TraceData(const ServiceElementInstanceIdentifierView service_element_instance_identifier_view,
                      const TracingRuntime::TracePointType trace_point,
                      const BindingType binding_type,
                      const std::pair<const void*, std::size_t>& local_data_chunk,
                      const score::cpp::optional<TracingRuntime::TracePointDataId> trace_point_data_id) noexcept
{
    auto* const tracing_runtime = impl::Runtime::getInstance().GetTracingRuntime();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(tracing_runtime != nullptr);
    return tracing_runtime->Trace(binding_type,
                                  service_element_instance_identifier_view,
                                  trace_point,
                                  trace_point_data_id,
                                  local_data_chunk.first,
                                  local_data_chunk.second);
}

ResultBlank TraceShmData(const BindingType binding_type,
                         const ServiceElementTracingData service_element_tracing_data,
                         const ServiceElementInstanceIdentifierView service_element_instance_identifier_view,
                         const TracingRuntime::TracePointType trace_point,
                         TracingRuntime::TracePointDataId trace_point_data_id,
                         TypeErasedSamplePtr sample_ptr,
                         const std::pair<const void*, std::size_t>& data_chunk) noexcept
{
    auto* const tracing_runtime = impl::Runtime::getInstance().GetTracingRuntime();
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(tracing_runtime != nullptr);

    return tracing_runtime->Trace(binding_type,
                                  service_element_tracing_data,
                                  service_element_instance_identifier_view,
                                  trace_point,
                                  trace_point_data_id,
                                  std::move(sample_ptr),
                                  data_chunk.first,
                                  data_chunk.second);
}

ServiceElementInstanceIdentifierView GetServiceElementInstanceIdentifierView(
    const InstanceIdentifier& instance_identifier,
    const score::cpp::string_view service_element_name,
    const ServiceElementType service_element_type) noexcept
{
    const auto instance_specifier_view = GetInstanceSpecifier(instance_identifier);
    const auto service_type = GetServiceType(instance_identifier);
    const ServiceElementIdentifierView service_element_identifier_view{
        score::cpp::string_view{service_type.data(), service_type.size()}, service_element_name, service_element_type};
    const ServiceElementInstanceIdentifierView service_element_instance_identifier_view{
        service_element_identifier_view,
        score::cpp::string_view{instance_specifier_view.data(), instance_specifier_view.size()}};
    return service_element_instance_identifier_view;
}

}  // namespace score::mw::com::impl::tracing
