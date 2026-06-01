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
#include "score/mw/com/impl/configuration/lola_field_instance_deployment.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"
#include "score/mw/com/impl/configuration/lola_event_instance_deployment.h"

#include <score/optional.hpp>
#include <utility>

namespace score::mw::com::impl
{

namespace
{

constexpr auto kUseGetIfAvailableKey = "useGetIfAvailable";
constexpr auto kUseSetIfAvailableKey = "useSetIfAvailable";

}  // namespace

LolaFieldInstanceDeployment::LolaFieldInstanceDeployment(LolaEventInstanceDeployment event_deployment,
                                                         const std::optional<bool> use_get_if_available,
                                                         const std::optional<bool> use_set_if_available) noexcept
    : lola_event_instance_deployment_{std::move(event_deployment)},
      use_get_if_available_{use_get_if_available},
      use_set_if_available_{use_set_if_available}
{
}

LolaFieldInstanceDeployment::LolaFieldInstanceDeployment(const score::json::Object& json_object) noexcept
    : LolaFieldInstanceDeployment(LolaFieldInstanceDeployment::CreateFromJson(json_object))
{
}

LolaFieldInstanceDeployment LolaFieldInstanceDeployment::CreateFromJson(const score::json::Object& json_object) noexcept
{
    // Delegate event-specific parsing to LolaEventInstanceDeployment (which also checks serialization version).
    auto event_deployment = LolaEventInstanceDeployment::CreateFromJson(json_object);

    const auto use_get_if_available = GetOptionalValueFromJson<bool>(json_object, kUseGetIfAvailableKey);
    const auto use_set_if_available = GetOptionalValueFromJson<bool>(json_object, kUseSetIfAvailableKey);

    return LolaFieldInstanceDeployment(std::move(event_deployment), use_get_if_available, use_set_if_available);
}

score::json::Object LolaFieldInstanceDeployment::Serialize() const noexcept
{
    auto json_object = lola_event_instance_deployment_.Serialize();

    if (use_get_if_available_.has_value())
    {
        json_object[kUseGetIfAvailableKey] = score::json::Any{use_get_if_available_.value()};
    }

    if (use_set_if_available_.has_value())
    {
        json_object[kUseSetIfAvailableKey] = score::json::Any{use_set_if_available_.value()};
    }

    return json_object;
}

bool operator==(const LolaFieldInstanceDeployment& lhs, const LolaFieldInstanceDeployment& rhs) noexcept
{
    const bool use_get_if_available_equal = (lhs.use_get_if_available_ == rhs.use_get_if_available_);
    const bool use_set_if_available_equal = (lhs.use_set_if_available_ == rhs.use_set_if_available_);
    // Adding Brackets to the expression does not give additional value since only one logical operator is used which
    // is independent of the execution order
    // coverity[autosar_cpp14_a5_2_6_violation]
    return ((lhs.lola_event_instance_deployment_ == rhs.lola_event_instance_deployment_) &&
            use_get_if_available_equal && use_set_if_available_equal);
}

}  // namespace score::mw::com::impl
