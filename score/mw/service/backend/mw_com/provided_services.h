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

#include "score/mw/service/provided_service_container.h"

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

/// @brief Minimal stub for config_daemon testing
/// @note This is at mw::service level to match BMW architecture
template <template <typename> class ServiceDecorator>
class ProvidedServices : public ProvidedServicesBase
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

    // Implement ProvidedServicesBase virtual methods
    std::size_t Count() const noexcept override
    {
        return services_.size();
    }

    void StartAll() override
    {
        // Stub implementation - do nothing
    }

    void StopAll() override
    {
        // Stub implementation - do nothing
    }

  private:
    std::vector<std::pair<std::string, ProvidedServiceHolder>> services_;
};

}  // namespace service
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_SERVICE_BACKEND_MW_COM_PROVIDED_SERVICES_H
