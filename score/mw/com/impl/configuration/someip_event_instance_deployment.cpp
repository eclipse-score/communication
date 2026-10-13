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
#include "score/mw/com/impl/configuration/someip_event_instance_deployment.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include <exception>
#include <optional>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kNumberOfSampleSlotsKeySomeIpEventInstDepl = "numberOfSampleSlots";
constexpr auto kSubscribersKeySomeIpEventInstDepl = "maxSubscribers";
constexpr auto kMaxConcurrentAllocationsKeySomeIpEventInstDepl = "maxConcurrentAllocations";
constexpr auto kEnforceMaxSamplesKeySomeIpEventInstDepl = "enforceMaxSamples";

}  // namespace

SomeIpEventInstanceDeployment::SomeIpEventInstanceDeployment(std::optional<SampleSlotCountType> number_of_sample_slots,
                                                             std::optional<SubscriberCountType> max_subscribers,
                                                             std::optional<std::uint8_t> max_concurrent_allocations,
                                                             const bool enforce_max_samples) noexcept
    : max_subscribers_{max_subscribers},
      max_concurrent_allocations_{max_concurrent_allocations},
      enforce_max_samples_{enforce_max_samples},
      number_of_sample_slots_{number_of_sample_slots}
{
}

SomeIpEventInstanceDeployment::SomeIpEventInstanceDeployment(const score::json::Object& json_object)
    : SomeIpEventInstanceDeployment(SomeIpEventInstanceDeployment::CreateFromJson(json_object))
{
}

SomeIpEventInstanceDeployment SomeIpEventInstanceDeployment::CreateFromJson(const score::json::Object& json_object)
{
    const auto serialization_version = GetValueFromJson<std::uint32_t>(json_object, kSerializationVersionKey);
    if (serialization_version != serializationVersion)
    {
        std::terminate();
    }

    const auto number_of_sample_slots =
        GetOptionalValueFromJson<SampleSlotCountType>(json_object, kNumberOfSampleSlotsKeySomeIpEventInstDepl);
    const auto max_subscribers =
        GetOptionalValueFromJson<SubscriberCountType>(json_object, kSubscribersKeySomeIpEventInstDepl);
    const auto max_concurrent_allocations =
        GetOptionalValueFromJson<std::uint8_t>(json_object, kMaxConcurrentAllocationsKeySomeIpEventInstDepl);
    const auto enforce_max_samples = GetValueFromJson<bool>(json_object, kEnforceMaxSamplesKeySomeIpEventInstDepl);

    return SomeIpEventInstanceDeployment(
        number_of_sample_slots, max_subscribers, max_concurrent_allocations, enforce_max_samples);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall not be called
//                                                                   implicitly"
// false positive std::bad_optional_access. We check with has_value() before accessing the optional.
// coverity[autosar_cpp14_a15_5_3_violation]
score::json::Object SomeIpEventInstanceDeployment::Serialize() const
{
    score::json::Object json_object{};
    if (number_of_sample_slots_.has_value())
    {
        json_object[kNumberOfSampleSlotsKeySomeIpEventInstDepl] = score::json::Any{number_of_sample_slots_.value()};
    }
    if (max_subscribers_.has_value())
    {
        json_object[kSubscribersKeySomeIpEventInstDepl] = score::json::Any{max_subscribers_.value()};
    }
    json_object[kSerializationVersionKey] = json::Any{serializationVersion};

    if (max_concurrent_allocations_.has_value())
    {
        json_object[kMaxConcurrentAllocationsKeySomeIpEventInstDepl] =
            score::json::Any{max_concurrent_allocations_.value()};
    }

    json_object[kEnforceMaxSamplesKeySomeIpEventInstDepl] = score::json::Any{enforce_max_samples_};

    return json_object;
}

auto SomeIpEventInstanceDeployment::GetNumberOfSampleSlots() const noexcept -> std::optional<SampleSlotCountType>
{
    return number_of_sample_slots_;
}

void SomeIpEventInstanceDeployment::SetNumberOfSampleSlots(SampleSlotCountType number_of_sample_slots) noexcept
{
    number_of_sample_slots_ = number_of_sample_slots;
}

bool operator==(const SomeIpEventInstanceDeployment& lhs, const SomeIpEventInstanceDeployment& rhs) noexcept
{
    const bool number_of_sample_slots_equal = (lhs.number_of_sample_slots_ == rhs.number_of_sample_slots_);
    const bool max_subscribers_equal = (lhs.max_subscribers_ == rhs.max_subscribers_);
    const bool max_concurrent_allocations_equal = (lhs.max_concurrent_allocations_ == rhs.max_concurrent_allocations_);
    const bool enforce_max_samples_equal = (lhs.enforce_max_samples_ == rhs.enforce_max_samples_);
    // Adding brackets to the expression does not give additional value since only one logical operator is used which
    // is independent of the execution order
    // coverity[autosar_cpp14_a5_2_6_violation]
    return (number_of_sample_slots_equal && max_subscribers_equal && max_concurrent_allocations_equal &&
            enforce_max_samples_equal);
}

}  // namespace score::mw::com::impl
