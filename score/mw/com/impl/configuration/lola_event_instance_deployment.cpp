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
#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include "score/mw/log/logging.h"

#include <score/optional.hpp>
#include <exception>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kNumberOfSampleSlotsKey = "numberOfSampleSlots";
constexpr auto kSubscribersKey = "maxSubscribers";
constexpr auto kMaxConcurrentAllocationsKey = "maxConcurrentAllocations";
constexpr auto kEnforceMaxSamplesKey = "enforceMaxSamples";
constexpr auto kNumberOfIpcTracingSlotsKey = "numberOfIpcTracingSlots";
constexpr LolaEventInstanceDeployment::TracingSlotSizeType kNumberOfIpcTracingSlotsDefault{0U};

}  // namespace

LolaEventInstanceDeployment::LolaEventInstanceDeployment(std::optional<SampleSlotCountType> number_of_sample_slots,
                                                         std::optional<SubscriberCountType> max_subscribers,
                                                         std::optional<std::uint8_t> max_concurrent_allocations,
                                                         const bool enforce_max_samples,
                                                         const TracingSlotSizeType number_of_tracing_slots) noexcept
    : max_subscribers_{max_subscribers},
      max_concurrent_allocations_{max_concurrent_allocations},
      enforce_max_samples_{enforce_max_samples},
      number_of_sample_slots_{number_of_sample_slots},
      number_of_tracing_slots_{number_of_tracing_slots}
{
}

LolaEventInstanceDeployment::LolaEventInstanceDeployment(const score::json::Object& json_object) noexcept
    : LolaEventInstanceDeployment(LolaEventInstanceDeployment::CreateFromJson(json_object))
{
}

LolaEventInstanceDeployment LolaEventInstanceDeployment::CreateFromJson(const score::json::Object& json_object) noexcept
{

    const auto serialization_version = GetValueFromJson<std::uint32_t>(json_object, kSerializationVersionKey);
    if (serialization_version != serializationVersion)
    {
        std::terminate();
    }

    const auto number_of_sample_slots =
        GetOptionalValueFromJson<SampleSlotCountType>(json_object, kNumberOfSampleSlotsKey);
    const auto max_subscribers = GetOptionalValueFromJson<SubscriberCountType>(json_object, kSubscribersKey);
    const auto max_concurrent_allocations =
        GetOptionalValueFromJson<std::uint8_t>(json_object, kMaxConcurrentAllocationsKey);
    const auto enforce_max_samples = GetValueFromJson<bool>(json_object, kEnforceMaxSamplesKey);
    const auto number_of_tracing_slots_opt =
        GetOptionalValueFromJson<TracingSlotSizeType>(json_object, kNumberOfIpcTracingSlotsKey);

    auto number_of_tracing_slots = number_of_tracing_slots_opt.value_or(kNumberOfIpcTracingSlotsDefault);

    return LolaEventInstanceDeployment(number_of_sample_slots,
                                       max_subscribers,
                                       max_concurrent_allocations,
                                       enforce_max_samples,
                                       number_of_tracing_slots);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// false positive std::bad_optional_access. We check and early exit in case the optional is empty.
// coverity[autosar_cpp14_a15_5_3_violation]
score::json::Object LolaEventInstanceDeployment::Serialize() const noexcept
{
    score::json::Object json_object{};
    if (number_of_sample_slots_.has_value())
    {
        json_object[kNumberOfSampleSlotsKey] = score::json::Any{number_of_sample_slots_.value()};
    }
    if (max_subscribers_.has_value())
    {
        json_object[kSubscribersKey] = score::json::Any{max_subscribers_.value()};
    }
    json_object[kSerializationVersionKey] = json::Any{serializationVersion};

    if (max_concurrent_allocations_.has_value())
    {
        json_object[kMaxConcurrentAllocationsKey] = score::json::Any{max_concurrent_allocations_.value()};
    }

    json_object[kEnforceMaxSamplesKey] = score::json::Any{enforce_max_samples_};

    // We always turn of ipc tracing. I.e., serialize  kNumberOfIpcTracingSlotsKey as false
    json_object[kNumberOfIpcTracingSlotsKey] = static_cast<std::uint8_t>(0U);

    return json_object;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// false positive std::bad_optional_access. We check and early exit in case the optional is empty.
// coverity[autosar_cpp14_a15_5_3_violation]
auto LolaEventInstanceDeployment::GetNumberOfSampleSlots() const noexcept -> std::optional<SampleSlotCountType>
{
    if (!number_of_sample_slots_.has_value())
    {
        return {};
    }

    const auto intermediate = static_cast<std::uint32_t>(number_of_sample_slots_.value()) +
                              static_cast<std::uint32_t>(number_of_tracing_slots_);
    if (intermediate > std::numeric_limits<std::uint16_t>::max())
    {
        ::score::mw::log::LogFatal("lola")
            << "Number of sample slots + number of tracing slots exceeds sample slot limit. Terminating.";
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(false);
    }
    return static_cast<std::uint16_t>(intermediate);
}

auto LolaEventInstanceDeployment::GetNumberOfSampleSlotsExcludingTracingSlot() const noexcept
    -> std::optional<SampleSlotCountType>
{
    if (!number_of_sample_slots_.has_value())
    {
        return {};
    }
    return *number_of_sample_slots_;
}

auto LolaEventInstanceDeployment::GetNumberOfTracingSlots() const noexcept -> TracingSlotSizeType
{
    return number_of_tracing_slots_;
}

void LolaEventInstanceDeployment::SetNumberOfSampleSlots(SampleSlotCountType number_of_sample_slots) noexcept
{

    number_of_sample_slots_ = number_of_sample_slots;
}

bool operator==(const LolaEventInstanceDeployment& lhs, const LolaEventInstanceDeployment& rhs) noexcept
{
    const bool number_of_sample_slots_equal = (lhs.number_of_sample_slots_ == rhs.number_of_sample_slots_);
    const bool number_of_tracing_slots_equal = (lhs.number_of_tracing_slots_ == rhs.number_of_tracing_slots_);
    const bool max_subscribers_equal = (lhs.max_subscribers_ == rhs.max_subscribers_);
    const bool max_concurrent_allocations_equal = (lhs.max_concurrent_allocations_ == rhs.max_concurrent_allocations_);
    const bool enforce_max_samples_equal = (lhs.enforce_max_samples_ == rhs.enforce_max_samples_);
    // Adding Brackets to the expression does not give additional value since only one logical operator is used which
    // is independent of the execution order
    // coverity[autosar_cpp14_a5_2_6_violation]
    return (number_of_sample_slots_equal && number_of_tracing_slots_equal && max_subscribers_equal &&
            max_concurrent_allocations_equal && enforce_max_samples_equal);
}

}  // namespace score::mw::com::impl
