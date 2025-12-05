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
#include "score/mw/com/impl/configuration/test/configuration_test_resources.h"

namespace score::mw::com::impl
{

namespace
{

const auto kDummyEventName1 = "dummy_event_1";
const auto kDummyEventName2 = "dummy_event_2";
const auto kDummyFieldName1 = "dummy_field_1";
const auto kDummyFieldName2 = "dummy_field_2";
const auto kDummyMethodName1 = "dummy_method_1";
const auto kDummyMethodName2 = "dummy_method_2";

}  // namespace

LolaEventInstanceDeployment MakeDefaultLolaEventInstanceDeployment() noexcept
{
    return LolaEventInstanceDeployment({}, {}, 1U, true, 0);
}

LolaEventInstanceDeployment MakeLolaEventInstanceDeployment(
    const std::optional<std::uint16_t> max_samples,
    const std::optional<std::uint8_t> max_subscribers,
    const std::optional<std::uint8_t> max_concurrent_allocations,
    const bool enforce_max_samples,
    std::uint8_t number_of_tracing_slots) noexcept
{
    const LolaEventInstanceDeployment unit{
        max_samples, max_subscribers, max_concurrent_allocations, enforce_max_samples, number_of_tracing_slots};
    return unit;
}

LolaFieldInstanceDeployment MakeLolaFieldInstanceDeployment(
    const std::uint16_t max_samples,
    const std::optional<std::uint8_t> max_subscribers,
    const std::optional<std::uint8_t> max_concurrent_allocations,
    bool enforce_max_samples,
    std::uint8_t number_of_tracing_slots) noexcept
{
    const LolaFieldInstanceDeployment unit{
        max_samples, max_subscribers, max_concurrent_allocations, enforce_max_samples, number_of_tracing_slots};
    return unit;
}

LolaMethodInstanceDeployment MakeDefaultLolaMethodInstanceDeployment() noexcept
{
    return LolaMethodInstanceDeployment{std::nullopt};
}

LolaMethodInstanceDeployment MakeLolaMethodInstanceDeployment(
    const std::optional<LolaMethodInstanceDeployment::QueueSize> queue_size) noexcept
{
    const LolaMethodInstanceDeployment unit{queue_size};
    return unit;
}

LolaServiceInstanceDeployment MakeLolaServiceInstanceDeployment(
    const score::cpp::optional<LolaServiceInstanceId> instance_id,
    const score::cpp::optional<std::size_t> shared_memory_size,
    const score::cpp::optional<std::size_t> control_asil_b_memory_size,
    const score::cpp::optional<std::size_t> control_qm_memory_size) noexcept
{
    const LolaEventInstanceDeployment event_instance_deployment_1{MakeLolaEventInstanceDeployment(12U, 13U)};
    const LolaEventInstanceDeployment event_instance_deployment_2{MakeLolaEventInstanceDeployment(14U, 15U)};

    const LolaFieldInstanceDeployment field_instance_deployment_1{MakeLolaFieldInstanceDeployment(16U, 17U)};
    const LolaFieldInstanceDeployment field_instance_deployment_2{MakeLolaFieldInstanceDeployment(18U, 19U)};

    const LolaMethodInstanceDeployment method_instance_deployment_1{MakeLolaMethodInstanceDeployment(20U)};
    const LolaMethodInstanceDeployment method_instance_deployment_2{MakeLolaMethodInstanceDeployment(21U)};

    const LolaServiceInstanceDeployment::EventInstanceMapping events{{kDummyEventName1, event_instance_deployment_1},
                                                                     {kDummyEventName2, event_instance_deployment_2}};

    const LolaServiceInstanceDeployment::FieldInstanceMapping fields{{kDummyFieldName1, field_instance_deployment_1},
                                                                     {kDummyFieldName2, field_instance_deployment_2}};

    const LolaServiceInstanceDeployment::MethodInstanceMapping methods{
        {kDummyMethodName1, method_instance_deployment_1}, {kDummyMethodName2, method_instance_deployment_2}};

    const std::unordered_map<QualityType, std::vector<uid_t>> allowed_consumer{
        {QualityType::kInvalid, {1U, 2U}}, {QualityType::kASIL_QM, {3U, 4U}}, {QualityType::kASIL_B, {5U, 6U}}};
    const std::unordered_map<QualityType, std::vector<uid_t>> allowed_provider{
        {QualityType::kInvalid, {7U, 8U}}, {QualityType::kASIL_QM, {9U, 10U}}, {QualityType::kASIL_B, {11U, 12U}}};

    LolaServiceInstanceDeployment unit{};
    unit.instance_id_ = instance_id;
    unit.shared_memory_size_ = shared_memory_size;
    unit.control_asil_b_memory_size_ = control_asil_b_memory_size;
    unit.control_qm_memory_size_ = control_qm_memory_size;
    unit.events_ = events;
    unit.fields_ = fields;
    unit.methods_ = methods;
    unit.allowed_consumer_ = allowed_consumer;
    unit.allowed_provider_ = allowed_provider;

    return unit;
}

SomeIpServiceInstanceDeployment MakeSomeIpServiceInstanceDeployment(
    const score::cpp::optional<SomeIpServiceInstanceId> instance_id) noexcept
{
    const SomeIpEventInstanceDeployment event_instance_deployment_1{};
    const SomeIpEventInstanceDeployment event_instance_deployment_2{};

    const SomeIpFieldInstanceDeployment field_instance_deployment_1{};
    const SomeIpFieldInstanceDeployment field_instance_deployment_2{};

    const SomeIpServiceInstanceDeployment::EventInstanceMapping events{{kDummyEventName1, event_instance_deployment_1},
                                                                       {kDummyEventName2, event_instance_deployment_2}};

    const SomeIpServiceInstanceDeployment::FieldInstanceMapping fields{{kDummyFieldName1, field_instance_deployment_1},
                                                                       {kDummyFieldName2, field_instance_deployment_2}};

    return SomeIpServiceInstanceDeployment(instance_id, events, fields);
}

LolaServiceTypeDeployment MakeLolaServiceTypeDeployment(const std::uint16_t service_id) noexcept
{
    const LolaEventId event_type_deployment_1{33U};
    const LolaEventId event_type_deployment_2{34U};

    const LolaFieldId field_type_deployment_1{35U};
    const LolaFieldId field_type_deployment_2{36U};

    const LolaMethodId method_type_deployment_1{37U};
    const LolaMethodId method_type_deployment_2{38U};

    const LolaServiceTypeDeployment::EventIdMapping events{{kDummyEventName1, event_type_deployment_1},
                                                           {kDummyEventName2, event_type_deployment_2}};

    const LolaServiceTypeDeployment::FieldIdMapping fields{{kDummyFieldName1, field_type_deployment_1},
                                                           {kDummyFieldName2, field_type_deployment_2}};

    const LolaServiceTypeDeployment::MethodIdMapping methods{{kDummyMethodName1, method_type_deployment_1},
                                                             {kDummyMethodName2, method_type_deployment_2}};

    LolaServiceTypeDeployment unit{service_id, events, fields, methods};

    return unit;
}

void ConfigurationStructsFixture::ExpectLolaEventInstanceDeploymentObjectsEqual(
    const LolaEventInstanceDeployment& lhs,
    const LolaEventInstanceDeployment& rhs) const noexcept
{
    EXPECT_EQ(lhs.max_subscribers_, rhs.max_subscribers_);
    EXPECT_EQ(lhs.max_concurrent_allocations_, rhs.max_concurrent_allocations_);
    EXPECT_EQ(lhs.enforce_max_samples_, rhs.enforce_max_samples_);
    EXPECT_EQ(lhs.GetNumberOfSampleSlotsExcludingTracingSlot(), rhs.GetNumberOfSampleSlotsExcludingTracingSlot());
}

void ConfigurationStructsFixture::ExpectLolaFieldInstanceDeploymentObjectsEqual(
    const LolaFieldInstanceDeployment& lhs,
    const LolaFieldInstanceDeployment& rhs) const noexcept
{
    EXPECT_EQ(lhs.max_subscribers_, rhs.max_subscribers_);
    EXPECT_EQ(lhs.max_concurrent_allocations_, rhs.max_concurrent_allocations_);
    EXPECT_EQ(lhs.enforce_max_samples_, rhs.enforce_max_samples_);
    EXPECT_EQ(lhs.GetNumberOfSampleSlotsExcludingTracingSlot(), rhs.GetNumberOfSampleSlotsExcludingTracingSlot());
}

void ConfigurationStructsFixture::ExpectLolaMethodInstanceDeploymentObjectsEqual(
    const LolaMethodInstanceDeployment& lhs,
    const LolaMethodInstanceDeployment& rhs) const noexcept
{
    EXPECT_EQ(lhs.queue_size_, rhs.queue_size_);
}

void ConfigurationStructsFixture::ExpectSomeIpEventInstanceDeploymentObjectsEqual(
    const SomeIpEventInstanceDeployment& /* lhs */,
    const SomeIpEventInstanceDeployment& /* rhs */) const noexcept
{
}

void ConfigurationStructsFixture::ExpectSomeIpFieldInstanceDeploymentObjectsEqual(
    const SomeIpFieldInstanceDeployment& /* lhs */,
    const SomeIpFieldInstanceDeployment& /* rhs */) const noexcept
{
}

void ConfigurationStructsFixture::ExpectLolaServiceInstanceDeploymentObjectsEqual(
    const LolaServiceInstanceDeployment& lhs,
    const LolaServiceInstanceDeployment& rhs) const noexcept
{
    EXPECT_EQ(lhs.instance_id_, rhs.instance_id_);
    EXPECT_EQ(lhs.shared_memory_size_, rhs.shared_memory_size_);
    EXPECT_EQ(lhs.control_asil_b_memory_size_, rhs.control_asil_b_memory_size_);
    EXPECT_EQ(lhs.control_qm_memory_size_, rhs.control_qm_memory_size_);

    ASSERT_EQ(lhs.events_.size(), rhs.events_.size());
    for (const auto& lhs_it : lhs.events_)
    {
        auto rhs_it = rhs.events_.find(lhs_it.first);
        ASSERT_NE(rhs_it, rhs.events_.end());
        ExpectLolaEventInstanceDeploymentObjectsEqual(lhs_it.second, rhs_it->second);
    }

    ASSERT_EQ(lhs.fields_.size(), rhs.fields_.size());
    for (const auto& lhs_it : lhs.fields_)
    {
        auto rhs_it = rhs.fields_.find(lhs_it.first);
        ASSERT_NE(rhs_it, rhs.fields_.end());
        ExpectLolaFieldInstanceDeploymentObjectsEqual(lhs_it.second, rhs_it->second);
    }

    ASSERT_EQ(lhs.methods_.size(), rhs.methods_.size());
    for (const auto& lhs_it : lhs.methods_)
    {
        auto rhs_it = rhs.methods_.find(lhs_it.first);
        ASSERT_NE(rhs_it, rhs.methods_.end());
        ExpectLolaMethodInstanceDeploymentObjectsEqual(lhs_it.second, rhs_it->second);
    }

    ASSERT_EQ(lhs.allowed_consumer_.size(), rhs.allowed_consumer_.size());
    for (const auto& lhs_consumer_it : lhs.allowed_consumer_)
    {
        auto rhs_consumer_it = rhs.allowed_consumer_.find(lhs_consumer_it.first);
        ASSERT_NE(rhs_consumer_it, rhs.allowed_consumer_.end());

        const auto& lhs_uid_vector = lhs_consumer_it.second;
        const auto& rhs_uid_vector = rhs_consumer_it->second;
        ASSERT_EQ(lhs_uid_vector.size(), rhs_uid_vector.size());
        for (std::size_t i = 0; i < lhs_uid_vector.size(); ++i)
        {
            EXPECT_EQ(lhs_uid_vector[i], rhs_uid_vector[i]);
        }
    }

    ASSERT_EQ(lhs.allowed_provider_.size(), rhs.allowed_provider_.size());
    for (const auto& lhs_provider_it : lhs.allowed_provider_)
    {
        auto rhs_provider_it = rhs.allowed_provider_.find(lhs_provider_it.first);
        ASSERT_NE(rhs_provider_it, rhs.allowed_provider_.end());

        const auto& lhs_uid_vector = lhs_provider_it.second;
        const auto& rhs_uid_vector = rhs_provider_it->second;
        ASSERT_EQ(lhs_uid_vector.size(), rhs_uid_vector.size());
        for (std::size_t i = 0; i < lhs_uid_vector.size(); ++i)
        {
            EXPECT_EQ(lhs_uid_vector[i], rhs_uid_vector[i]);
        }
    }
}

void ConfigurationStructsFixture::ExpectSomeIpServiceInstanceDeploymentObjectsEqual(
    const SomeIpServiceInstanceDeployment& lhs,
    const SomeIpServiceInstanceDeployment& rhs) const noexcept
{
    EXPECT_EQ(lhs.instance_id_, rhs.instance_id_);

    ASSERT_EQ(lhs.events_.size(), rhs.events_.size());
    for (const auto& lhs_it : lhs.events_)
    {
        auto rhs_it = rhs.events_.find(lhs_it.first);
        ASSERT_NE(rhs_it, rhs.events_.end());
        ExpectSomeIpEventInstanceDeploymentObjectsEqual(lhs_it.second, rhs_it->second);
    }

    ASSERT_EQ(lhs.fields_.size(), rhs.fields_.size());
    for (const auto& lhs_it : lhs.fields_)
    {
        auto rhs_it = rhs.fields_.find(lhs_it.first);
        ASSERT_NE(rhs_it, rhs.fields_.end());
        ExpectSomeIpFieldInstanceDeploymentObjectsEqual(lhs_it.second, rhs_it->second);
    }
}

void ConfigurationStructsFixture::ExpectServiceInstanceDeploymentObjectsEqual(
    const ServiceInstanceDeployment& lhs,
    const ServiceInstanceDeployment& rhs) const noexcept
{
    EXPECT_EQ(lhs.asilLevel_, rhs.asilLevel_);
    ExpectServiceIdentifierTypeObjectsEqual(lhs.service_, rhs.service_);
    ASSERT_EQ(lhs.bindingInfo_.index(), rhs.bindingInfo_.index());

    auto visitor = score::cpp::overload(
        [this, rhs](const LolaServiceInstanceDeployment& lhs_deployment) {
            const auto* const rhs_deployment = std::get_if<LolaServiceInstanceDeployment>(&rhs.bindingInfo_);
            ASSERT_NE(rhs_deployment, nullptr);
            ExpectLolaServiceInstanceDeploymentObjectsEqual(lhs_deployment, *rhs_deployment);
        },
        [this, lhs, rhs](const SomeIpServiceInstanceDeployment& lhs_deployment) {
            const auto* const rhs_deployment = std::get_if<SomeIpServiceInstanceDeployment>(&rhs.bindingInfo_);
            ASSERT_NE(rhs_deployment, nullptr);
            ExpectSomeIpServiceInstanceDeploymentObjectsEqual(lhs_deployment, *rhs_deployment);
        },
        [](const score::cpp::blank&) noexcept {});
    std::visit(visitor, lhs.bindingInfo_);
}

void ConfigurationStructsFixture::ExpectLolaServiceTypeDeploymentObjectsEqual(
    const LolaServiceTypeDeployment& lhs,
    const LolaServiceTypeDeployment& rhs) const noexcept
{
    EXPECT_EQ(lhs.service_id_, rhs.service_id_);

    ASSERT_EQ(lhs.events_.size(), rhs.events_.size());
    for (auto lhs_it : lhs.events_)
    {
        auto rhs_it = rhs.events_.find(lhs_it.first);
        ASSERT_NE(rhs_it, rhs.events_.end());
        EXPECT_EQ(lhs_it.second, rhs_it->second);
    }

    ASSERT_EQ(lhs.fields_.size(), rhs.fields_.size());
    for (auto lhs_it : lhs.fields_)
    {
        auto rhs_it = rhs.fields_.find(lhs_it.first);
        ASSERT_NE(rhs_it, rhs.fields_.end());
        EXPECT_EQ(lhs_it.second, rhs_it->second);
    }

    ASSERT_EQ(lhs.methods_.size(), rhs.methods_.size());
    for (auto lhs_it : lhs.methods_)
    {
        auto rhs_it = rhs.methods_.find(lhs_it.first);
        ASSERT_NE(rhs_it, rhs.methods_.end());
        EXPECT_EQ(lhs_it.second, rhs_it->second);
    }
}

void ConfigurationStructsFixture::ExpectServiceTypeDeploymentObjectsEqual(
    const ServiceTypeDeployment& lhs,
    const ServiceTypeDeployment& rhs) const noexcept
{
    auto visitor = score::cpp::overload(
        [this, rhs](const LolaServiceTypeDeployment& lhs_deployment) {
            const auto* const rhs_deployment = std::get_if<LolaServiceTypeDeployment>(&rhs.binding_info_);
            ASSERT_NE(rhs_deployment, nullptr);
            ExpectLolaServiceTypeDeploymentObjectsEqual(lhs_deployment, *rhs_deployment);
        },
        [](const score::cpp::blank&) noexcept {});
    std::visit(visitor, lhs.binding_info_);
}

void ConfigurationStructsFixture::ExpectServiceVersionTypeObjectsEqual(const ServiceVersionType& lhs,
                                                                       const ServiceVersionType& rhs) const noexcept
{
    const auto lhs_view = ServiceVersionTypeView{lhs};
    const auto rhs_view = ServiceVersionTypeView{rhs};
    EXPECT_EQ(lhs_view.getMajor(), rhs_view.getMajor());
    EXPECT_EQ(lhs_view.getMinor(), rhs_view.getMinor());
}

void ConfigurationStructsFixture::ExpectServiceIdentifierTypeObjectsEqual(
    const ServiceIdentifierType& lhs,
    const ServiceIdentifierType& rhs) const noexcept
{
    const auto lhs_view = ServiceIdentifierTypeView{lhs};
    const auto rhs_view = ServiceIdentifierTypeView{rhs};
    EXPECT_EQ(lhs_view.getInternalTypeName(), rhs_view.getInternalTypeName());
    ExpectServiceVersionTypeObjectsEqual(lhs_view.GetVersion(), rhs_view.GetVersion());
}

void ConfigurationStructsFixture::ExpectServiceInstanceIdObjectsEqual(const ServiceInstanceId& lhs,
                                                                      const ServiceInstanceId& rhs) const noexcept
{
    auto visitor = score::cpp::overload(
        [this, rhs](const LolaServiceInstanceId& lhs_instance_id) {
            const auto* const rhs_instance_id = std::get_if<LolaServiceInstanceId>(&rhs.binding_info_);
            ASSERT_NE(rhs_instance_id, nullptr);
            ExpectLolaServiceInstanceIdObjectsEqual(lhs_instance_id, *rhs_instance_id);
        },
        [this, rhs](const SomeIpServiceInstanceId& lhs_instance_id) {
            const auto* const rhs_instance_id = std::get_if<SomeIpServiceInstanceId>(&rhs.binding_info_);
            ASSERT_NE(rhs_instance_id, nullptr);
            ExpectSomeIpServiceInstanceIdObjectsEqual(lhs_instance_id, *rhs_instance_id);
        },
        [](const score::cpp::blank&) noexcept {});
    std::visit(visitor, lhs.binding_info_);
}

void ConfigurationStructsFixture::ExpectLolaServiceInstanceIdObjectsEqual(
    const LolaServiceInstanceId& lhs,
    const LolaServiceInstanceId& rhs) const noexcept
{
    EXPECT_EQ(lhs.GetId(), rhs.GetId());
}

void ConfigurationStructsFixture::ExpectSomeIpServiceInstanceIdObjectsEqual(
    const SomeIpServiceInstanceId& lhs,
    const SomeIpServiceInstanceId& rhs) const noexcept
{
    EXPECT_EQ(lhs.GetId(), rhs.GetId());
}

}  // namespace score::mw::com::impl
