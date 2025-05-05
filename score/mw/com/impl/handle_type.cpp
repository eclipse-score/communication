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
#include "score/mw/com/impl/handle_type.h"

#include "score/mw/log/logging.h"

#include <score/overload.hpp>

#include <exception>
#include <tuple>
#include <utility>

namespace score::mw::com::impl
{

namespace
{

ServiceInstanceId ExtractInstanceId(score::cpp::optional<ServiceInstanceId> instance_id,
                                    const InstanceIdentifier& identifier) noexcept
{
    if (instance_id.has_value())
    {
        return instance_id.value();
    }
    else
    {
        const InstanceIdentifierView instance_id_view{identifier};
        const auto service_instance_id_result = instance_id_view.GetServiceInstanceId();
        if (!service_instance_id_result.has_value())
        {
            ::score::mw::log::LogFatal("lola")
                << "Service instance ID must be provided to the constructor of HandleType if it "
                   "isn't specified in the configuration. Exiting";
            std::terminate();
        }
        return *service_instance_id_result;
    }
}

}  // namespace

HandleType::HandleType(InstanceIdentifier identifier, score::cpp::optional<ServiceInstanceId> instance_id) noexcept
    : identifier_{std::move(identifier)}, instance_id_{ExtractInstanceId(instance_id, identifier_)}
{
}

auto HandleType::GetInstanceIdentifier() const noexcept -> const InstanceIdentifier&
{
    return this->identifier_;
}

auto HandleType::GetServiceInstanceDeployment() const noexcept -> const ServiceInstanceDeployment&
{
    const InstanceIdentifierView instance_id{GetInstanceIdentifier()};
    return instance_id.GetServiceInstanceDeployment();
}

auto HandleType::GetServiceTypeDeployment() const noexcept -> const ServiceTypeDeployment&
{
    const InstanceIdentifierView instance_id{GetInstanceIdentifier()};
    return instance_id.GetServiceTypeDeployment();
}

auto operator==(const HandleType& lhs, const HandleType& rhs) noexcept -> bool
{
    return ((lhs.identifier_ == rhs.identifier_) && (lhs.instance_id_ == rhs.instance_id_));
}

auto operator<(const HandleType& lhs, const HandleType& rhs) noexcept -> bool
{
    return std::tie(lhs.identifier_, lhs.instance_id_) < std::tie(rhs.identifier_, rhs.instance_id_);
}

auto make_HandleType(InstanceIdentifier identifier, score::cpp::optional<ServiceInstanceId> instance_id) noexcept -> HandleType
{
    return HandleType(std::move(identifier), instance_id);
}

}  // namespace score::mw::com::impl
