// *******************************************************************************
// Copyright (c) 2026 Contributors to the Eclipse Foundation
//
// See the NOTICE file(s) distributed with this work for additional
// information regarding copyright ownership.
//
// This program and the accompanying materials are made available under the
// terms of the Apache License Version 2.0 which is available at
// https://www.apache.org/licenses/LICENSE-2.0
//
// SPDX-License-Identifier: Apache-2.0
// *******************************************************************************

#ifndef SCORE_MW_SERVICE_BACKEND_MW_COM_PROVIDED_SERVICES_H
#define SCORE_MW_SERVICE_BACKEND_MW_COM_PROVIDED_SERVICES_H

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace score
{
namespace mw
{
namespace service
{
class ProvidedService;

namespace backend
{
namespace mw_com
{

/// @brief Minimal stub for config_daemon testing
template <template <typename> class ServiceDecorator>
class ProvidedServices
{
    using ProvidedServiceHolder = std::unique_ptr<ProvidedService>;

  public:
    ProvidedServices() = default;

    /// @brief Add a service
    template <typename ServiceType, typename... Args>
    ProvidedServices& Add(Args&&... args) &
    {
        using DecoratorType = ServiceDecorator<ServiceType>;
        services_.emplace_back(
            std::string{},
            std::make_unique<DecoratorType>(DecoratorType::template Create<ServiceType>(std::forward<Args>(args)...)));
        return *this;
    }

    /// @brief Add a service (rvalue)
    template <typename ServiceType, typename... Args>
    ProvidedServices&& Add(Args&&... args) &&
    {
        return std::move(this->Add<ServiceType>(std::forward<Args>(args)...));
    }

    /// @brief Check if service exists
    template <typename ServiceType>
    bool Has() const noexcept
    {
        return Get<ServiceType>() != nullptr;
    }

    /// @brief Get service
    template <typename ServiceType>
    ServiceType* Get() noexcept
    {
        for (auto& [_, service_holder] : services_)
        {
            if (service_holder)
            {
                if (auto* decorator = dynamic_cast<ServiceDecorator<ServiceType>*>(service_holder.get()))
                {
                    return decorator->GetService();
                }
            }
        }
        return nullptr;
    }

    /// @brief Get service (const)
    template <typename ServiceType>
    const ServiceType* Get() const noexcept
    {
        for (const auto& [_, service_holder] : services_)
        {
            if (service_holder)
            {
                if (const auto* decorator = dynamic_cast<const ServiceDecorator<ServiceType>*>(service_holder.get()))
                {
                    return decorator->GetService();
                }
            }
        }
        return nullptr;
    }

    std::size_t size() const noexcept
    {
        return services_.size();
    }

  private:
    std::vector<std::pair<std::string, ProvidedServiceHolder>> services_;
};

}  // namespace mw_com
}  // namespace backend
}  // namespace service
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_SERVICE_BACKEND_MW_COM_PROVIDED_SERVICES_H
