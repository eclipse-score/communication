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
#include "score/mw/com/impl/configuration/lola_method_instance_deployment.h"

#include "score/mw/com/impl/configuration/configuration_common_resources.h"

#include <limits>
#include <string_view>

namespace score::mw::com::impl
{

namespace
{
using std::string_view_literals::operator""sv;
constexpr auto kQueueSizeKey = "queueSize"sv;
constexpr auto kMethodEnabledKey = "use"sv;
}  // namespace

LolaMethodInstanceDeployment::LolaMethodInstanceDeployment(std::optional<QueueSize> queue_size,
                                                           MethodEnabledType enabled)
    : queue_size_{queue_size}, enabled_{enabled}
{
}

LolaMethodInstanceDeployment::LolaMethodInstanceDeployment(
    const score::json::Object& serialized_lola_method_instance_deployment)
    : queue_size_{std::nullopt}, enabled_{}
{
    queue_size_ = GetOptionalValueFromJson<QueueSize>(serialized_lola_method_instance_deployment, kQueueSizeKey);
    enabled_ = GetValueFromJson<MethodEnabledType>(serialized_lola_method_instance_deployment, kMethodEnabledKey);
}

LolaMethodInstanceDeployment LolaMethodInstanceDeployment::CreateFromJson(
    const score::json::Object& serialized_lola_method_instance_deployment)
{
    return LolaMethodInstanceDeployment{serialized_lola_method_instance_deployment};
}

score::json::Object LolaMethodInstanceDeployment::Serialize() const
{
    score::json::Object json_object;
    if (queue_size_.has_value())
    {
        json_object[kQueueSizeKey] = score::json::Any{queue_size_.value()};
    }
    json_object[kMethodEnabledKey] = score::json::Any{enabled_};

    return json_object;
}

}  // namespace score::mw::com::impl
