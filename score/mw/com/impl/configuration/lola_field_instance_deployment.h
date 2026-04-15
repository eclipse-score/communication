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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_LOLA_FIELD_INSTANCE_DEPLOYMENT_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_LOLA_FIELD_INSTANCE_DEPLOYMENT_H

#include "score/json/json_parser.h"

#include <score/optional.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace score::mw::com::impl
{

class LolaFieldInstanceDeployment
{
  public:
    using SampleSlotCountType = std::uint16_t;
    using SubscriberCountType = std::uint8_t;
    using TracingSlotSizeType = std::uint8_t;

    explicit LolaFieldInstanceDeployment(std::optional<SampleSlotCountType> number_of_sample_slots,
                                         std::optional<SubscriberCountType> max_subscribers,
                                         std::optional<std::uint8_t> max_concurrent_allocations,
                                         const bool enforce_max_samples,
                                         const TracingSlotSizeType number_of_tracing_slots,
                                         const bool use_get_if_available,
                                         const bool use_set_if_available) noexcept;

    explicit LolaFieldInstanceDeployment(const score::json::Object& json_object) noexcept;

    static LolaFieldInstanceDeployment CreateFromJson(const score::json::Object& json_object) noexcept;

    score::json::Object Serialize() const noexcept;

    void SetNumberOfSampleSlots(SampleSlotCountType number_of_sample_slots) noexcept;

    [[nodiscard]] std::optional<SampleSlotCountType> GetNumberOfSampleSlots() const noexcept;
    [[nodiscard]] std::optional<SampleSlotCountType> GetNumberOfSampleSlotsExcludingTracingSlot() const noexcept;

    [[nodiscard]] TracingSlotSizeType GetNumberOfTracingSlots() const noexcept;

    /// \brief max subscribers slots is only relevant/required on skeleton side. On the proxy side it is irrelevant.
    ///         Therefore, it is optional!
    // Note the struct is not compliant to POD type containing non-POD member.
    // The struct is used as a config storage obtained by performing the parsing json object.
    // Public access is more convenient to reach the following members of the struct.
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::optional<SubscriberCountType> max_subscribers_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::optional<std::uint8_t> max_concurrent_allocations_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    bool enforce_max_samples_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    bool use_get_if_available_{false};
    // coverity[autosar_cpp14_m11_0_1_violation]
    bool use_set_if_available_{false};

    // False positive, variable is used outside of the file.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    constexpr static std::uint32_t serializationVersion = 1U;

    friend bool operator==(const LolaFieldInstanceDeployment& lhs, const LolaFieldInstanceDeployment& rhs) noexcept;

  private:
    /// \brief number of sample slots is only relevant/required on skeleton side, where slots get allocated. On the
    ///         proxy side it is irrelevant. Therefore, it is optional!
    std::optional<SampleSlotCountType> number_of_sample_slots_;
    // Non-zero values greater than one for this parameter only make sense on the skeleton side. For the proxy it is
    // just important if the tracing is enabled or not, i.e., if this variable is zero or non-zero.
    TracingSlotSizeType number_of_tracing_slots_;

};

bool operator==(const LolaFieldInstanceDeployment& lhs, const LolaFieldInstanceDeployment& rhs) noexcept;


}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_LOLA_FIELD_INSTANCE_DEPLOYMENT_H
