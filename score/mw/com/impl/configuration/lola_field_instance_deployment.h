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
#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"

#include <score/optional.hpp>

#include <cstdint>
#include <optional>

namespace score::mw::com::impl
{

class LolaFieldInstanceDeployment
{
  public:
    explicit LolaFieldInstanceDeployment(LolaEventInstanceDeployment event_deployment,
                                         const std::optional<bool> use_get_if_available,
                                         const std::optional<bool> use_set_if_available) noexcept;

    explicit LolaFieldInstanceDeployment(const score::json::Object& json_object) noexcept;

    static LolaFieldInstanceDeployment CreateFromJson(const score::json::Object& json_object) noexcept;

    score::json::Object Serialize() const noexcept;

    void SetNumberOfSampleSlots(LolaEventInstanceDeployment::SampleSlotCountType number_of_sample_slots) noexcept;

    [[nodiscard]] std::optional<LolaEventInstanceDeployment::SampleSlotCountType> GetNumberOfSampleSlots()
        const noexcept;
    [[nodiscard]] std::optional<LolaEventInstanceDeployment::SampleSlotCountType>
    GetNumberOfSampleSlotsExcludingTracingSlot() const noexcept;

    [[nodiscard]] LolaEventInstanceDeployment::TracingSlotSizeType GetNumberOfTracingSlots() const noexcept;

    // Note the struct is not compliant to POD type containing non-POD member.
    // The struct is used as a config storage obtained by performing the parsing json object.
    // Public access is more convenient to reach the following members of the struct.
    // coverity[autosar_cpp14_m11_0_1_violation]
    LolaEventInstanceDeployment lola_event_instance_deployment_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::optional<bool> use_get_if_available_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::optional<bool> use_set_if_available_;

    // False positive, variable is used outside of the file.
    // coverity[autosar_cpp14_a0_1_1_violation : FALSE]
    constexpr static std::uint32_t serializationVersion = 1U;

    friend bool operator==(const LolaFieldInstanceDeployment& lhs, const LolaFieldInstanceDeployment& rhs) noexcept;
};

bool operator==(const LolaFieldInstanceDeployment& lhs, const LolaFieldInstanceDeployment& rhs) noexcept;

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_LOLA_FIELD_INSTANCE_DEPLOYMENT_H
