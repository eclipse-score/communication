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
#include "score/mw/com/impl/bindings/lola/test/skeleton_event_test_resources.h"

#include "score/mw/com/impl/bindings/lola/skeleton_event_properties.h"

namespace score::mw::com::impl::lola
{

namespace
{

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;

}  // namespace

SkeletonEventFixture::SkeletonEventFixture() : SkeletonMockedMemoryFixture{}
{
    ON_CALL(lola_runtime_mock_, GetLolaMessaging).WillByDefault(ReturnRef(message_passing_service_mock_));
    ON_CALL(runtime_mock_, GetServiceDiscovery()).WillByDefault(ReturnRef(service_discovery_mock_));

    InitialiseSkeleton(GetValidInstanceIdentifier());

    SkeletonBinding::SkeletonEventBindings events{};
    SkeletonBinding::SkeletonFieldBindings fields{};
    std::optional<SkeletonBinding::RegisterShmObjectTraceCallback> register_shm_object_trace_callback{};
    skeleton_->PrepareOffer(events, fields, std::move(register_shm_object_trace_callback));
}

void SkeletonEventFixture::InitialiseSkeletonEvent(const ElementFqId element_fq_id,
                                                   const std::string& service_element_name,
                                                   const std::size_t max_samples,
                                                   const std::uint8_t max_subscribers,
                                                   const bool enforce_max_samples,
                                                   impl::tracing::SkeletonEventTracingData skeleton_event_tracing_data)
{
    skeleton_event_ = std::make_unique<SkeletonEvent<test::TestSampleType>>(
        *skeleton_,
        element_fq_id,
        service_element_name,
        SkeletonEventProperties{max_samples, max_subscribers, enforce_max_samples},
        skeleton_event_tracing_data);
}

EventControl* SkeletonEventFixture::GetEventControl(const ElementFqId element_fq_id,
                                                    const QualityType quality_type) const noexcept
{
    auto* service_data_control = SkeletonAttorney{*skeleton_}.GetServiceDataControl(quality_type);
    if (service_data_control == nullptr)
    {
        return nullptr;
    }
    const auto event_entry = service_data_control->event_controls_.find(element_fq_id);
    if (event_entry != service_data_control->event_controls_.end())
    {
        return &event_entry->second;
    }
    return nullptr;
}

InstanceIdentifier SkeletonEventFixture::GetValidInstanceIdentifier()
{
    return make_InstanceIdentifier(valid_asil_instance_deployment_, valid_type_deployment_);
}

}  // namespace score::mw::com::impl::lola
