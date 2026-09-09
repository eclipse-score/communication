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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_EVENT_INSTANCE_DEPLOYMENT_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_EVENT_INSTANCE_DEPLOYMENT_H

#include "score/json/json_parser.h"

#include <cstdint>
#include <optional>

namespace score::mw::com::impl
{

/// \brief Per event/field instance deployment information of the SOME/IP binding.
///
/// \details Mirrors LolaEventInstanceDeployment, minus the members which only make sense for a shared-memory based
///          binding (i.e. the IPC tracing slots).
class SomeIpEventInstanceDeployment
{
  public:
    using SampleSlotCountType = std::uint16_t;
    using SubscriberCountType = std::uint8_t;

    explicit SomeIpEventInstanceDeployment(std::optional<SampleSlotCountType> number_of_sample_slots,
                                           std::optional<SubscriberCountType> max_subscribers,
                                           std::optional<std::uint8_t> max_concurrent_allocations,
                                           const bool enforce_max_samples) noexcept;

    explicit SomeIpEventInstanceDeployment(const score::json::Object& json_object);

    static SomeIpEventInstanceDeployment CreateFromJson(const score::json::Object& json_object);

    score::json::Object Serialize() const;

    void SetNumberOfSampleSlots(SampleSlotCountType number_of_sample_slots) noexcept;

    [[nodiscard]] std::optional<SampleSlotCountType> GetNumberOfSampleSlots() const noexcept;

    /// \brief max subscribers is only relevant/required on skeleton side. On the proxy side it is irrelevant.
    ///        Therefore, it is optional!
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::optional<SubscriberCountType> max_subscribers_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::optional<std::uint8_t> max_concurrent_allocations_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    bool enforce_max_samples_;

    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    constexpr static std::uint32_t serializationVersion = 1U;

    friend bool operator==(const SomeIpEventInstanceDeployment& lhs,
                           const SomeIpEventInstanceDeployment& rhs) noexcept;

  private:
    /// \brief number of sample slots is only relevant/required on skeleton side, where slots get allocated. On the
    ///        proxy side it is irrelevant. Therefore, it is optional!
    std::optional<SampleSlotCountType> number_of_sample_slots_;
};

bool operator==(const SomeIpEventInstanceDeployment& lhs, const SomeIpEventInstanceDeployment& rhs) noexcept;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_SOMEIP_EVENT_INSTANCE_DEPLOYMENT_H
