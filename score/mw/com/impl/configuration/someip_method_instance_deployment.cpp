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
#include "score/mw/com/impl/configuration/someip_method_instance_deployment.h"

#include <limits>
#include <string_view>

namespace score::mw::com::impl
{

namespace
{
using std::string_view_literals::operator""sv;
constexpr auto kQueueSizeKey = "queueSize"sv;
}  // namespace

SomeIpMethodInstanceDeployment::SomeIpMethodInstanceDeployment(std::optional<QueueSize> queue_size)
    : queue_size_{queue_size}
{
}

SomeIpMethodInstanceDeployment::SomeIpMethodInstanceDeployment(
    const score::json::Object& serialized_someip_method_instance_deployment)
{
    const auto queue_size_iter = serialized_someip_method_instance_deployment.find(kQueueSizeKey.data());
    if (queue_size_iter != serialized_someip_method_instance_deployment.cend())
    {
        queue_size_ = queue_size_iter->second.As<QueueSize>().value();
    }
}

SomeIpMethodInstanceDeployment SomeIpMethodInstanceDeployment::CreateFromJson(
    const score::json::Object& serialized_someip_method_instance_deployment)
{
    return SomeIpMethodInstanceDeployment{serialized_someip_method_instance_deployment};
}

score::json::Object SomeIpMethodInstanceDeployment::Serialize() const
{
    score::json::Object result;
    if (queue_size_.has_value())
    {
        result[kQueueSizeKey.data()] = score::json::Any{queue_size_.value()};
    }
    return result;
}

}  // namespace score::mw::com::impl
