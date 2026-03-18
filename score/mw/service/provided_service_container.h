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

#ifndef SCORE_MW_SERVICE_PROVIDED_SERVICE_CONTAINER_H
#define SCORE_MW_SERVICE_PROVIDED_SERVICE_CONTAINER_H

#include "score/mw/service/provided_service.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace score
{
namespace mw
{
namespace service
{

class ProvidedServicesBase
{
  public:
    constexpr ProvidedServicesBase() noexcept = default;

    constexpr ProvidedServicesBase& operator=(const ProvidedServicesBase&) & = delete;
    constexpr ProvidedServicesBase(const ProvidedServicesBase&) = delete;

    virtual ~ProvidedServicesBase() noexcept = default;

    virtual std::size_t Count() const noexcept = 0;
    virtual void StartAll() = 0;
    virtual void StopAll() = 0;

  protected:
    ProvidedServicesBase& operator=(ProvidedServicesBase&&) & noexcept = default;
    constexpr ProvidedServicesBase(ProvidedServicesBase&&) noexcept = default;
};

template <template <typename> class ServiceDecorator>
class ProvidedServices final : public ProvidedServicesBase
{
    using ProvidedServiceHolder = std::unique_ptr<ProvidedService>;
    using InstanceSpecifierType = std::string;
    using InstanceSpecifierView = std::string_view;

  public:
    constexpr explicit ProvidedServices() noexcept = default;
    ~ProvidedServices() noexcept override
    {
        ProvidedServices::StopAll();
    }

    constexpr ProvidedServices& operator=(ProvidedServices&& other) & noexcept = default;
    constexpr ProvidedServices& operator=(const ProvidedServices&) & = delete;
    constexpr ProvidedServices(ProvidedServices&&) noexcept = default;
    constexpr ProvidedServices(const ProvidedServices&) = delete;

    template <typename ServiceType, typename... Args>
    ProvidedServices& Add(Args&&... args) &
    {
        return *this;
    }

    template <typename ServiceType, typename... Args>
    ProvidedServices&& Add(Args&&... args) &&
    {
        return std::move(*this);
    }

    template <typename ServiceType, typename... Args>
    ProvidedServices& AddViaInstanceSpecifier(InstanceSpecifierView instance_specifier, Args&&... args) &
    {
        return *this;
    }

    template <typename ServiceType, typename... Args>
    ProvidedServices&& AddViaInstanceSpecifier(InstanceSpecifierView instance_specifier, Args&&... args) &&
    {
        return std::move(*this);
    }

    template <typename ServiceBaseType, typename ServiceImplType, typename... Args>
    ProvidedServices& EmplaceServiceInstance(std::in_place_type_t<ServiceImplType>, Args&&... args) &
    {
        return *this;
    }

    template <typename ServiceBaseType, typename ServiceImplType, typename... Args>
    ProvidedServices&& EmplaceServiceInstance(std::in_place_type_t<ServiceImplType>, Args&&... args) &&
    {
        return std::move(*this);
    }

    template <typename ServiceBaseType, typename ServiceImplType, typename... Args>
    ProvidedServices& EmplaceServiceInstance(InstanceSpecifierView instance_specifier,
                                             std::in_place_type_t<ServiceImplType>,
                                             Args&&... args)
    {
        return *this;
    }

    template <typename ServiceType>
    auto Extract() noexcept
    {
        return Extract<ServiceType>(InstanceSpecifierView{});
    }

    /// @brief Extract the first service instance matching a particular InstanceSpecifier
    /// @tparam ServiceType the expected (implementation) type of the service instance to be extracted
    /// @param instance_specifier unique identifier for the service instance
    /// @return a valid smartpointer in case the dynamic_cast to `ServiceType` succeeds, nullptr otherwise
    template <typename ServiceType>
    auto Extract(InstanceSpecifierView instance_specifier) noexcept
    {
        return nullptr;
    }

    template <typename ServiceType>
    const ServiceType* Get() const noexcept
    {
        return nullptr;
    }
    template <typename ServiceType>
    ServiceType* Get() noexcept
    {
        return nullptr;
    }

    template <typename ServiceType>
    const ServiceType* Get(InstanceSpecifierView instance_specifier) const noexcept
    {
        return nullptr;
    }

    template <typename ServiceType>
    ServiceType* Get(InstanceSpecifierView instance_specifier) noexcept
    {
        return nullptr;
    }

    template <typename ServiceType>
    bool Has() const noexcept
    {
        return false;
    }

    bool Has(InstanceSpecifierView instance_specifier) const noexcept
    {
        return false;
    }

    template <typename ServiceType>
    bool Has(InstanceSpecifierView instance_specifier) const noexcept
    {
        return false;
    }

    std::size_t Count() const noexcept override
    {
        return 0;
    }

    /// @brief Start all contained valid service instances
    void StartAll() override {}

    /// @brief Stop all contained valid service instances
    void StopAll() override {}

    /// @brief Swap the content of this object with another one
    void Swap(ProvidedServices& other) noexcept {}
};

class ProvidedServiceContainer
{
  public:
    constexpr explicit ProvidedServiceContainer() noexcept = default;

    template <template <typename> class ServiceDecorator>
    ProvidedServiceContainer(ProvidedServices<ServiceDecorator> services)
        : services_{std::make_unique<decltype(services)>(std::move(services))}
    {
    }

    ProvidedServiceContainer& operator=(ProvidedServiceContainer&&) & noexcept = default;
    ProvidedServiceContainer& operator=(const ProvidedServiceContainer&) & = delete;
    ProvidedServiceContainer(ProvidedServiceContainer&&) noexcept = default;
    ProvidedServiceContainer(const ProvidedServiceContainer&) = delete;

    ~ProvidedServiceContainer() noexcept = default;

    template <template <typename> class ServiceDecorator>
    const auto* GetServices() const noexcept
    {
        return dynamic_cast<const ProvidedServices<ServiceDecorator>*>(services_.get());
    }
    template <template <typename> class ServiceDecorator>
    auto* GetServices() noexcept
    {
        return dynamic_cast<ProvidedServices<ServiceDecorator>*>(services_.get());
    }

    std::size_t NumServices() const noexcept
    {
        if (services_ == nullptr)
        {
            return 0U;
        }
        return services_->Count();
    }
    void StartServices() noexcept
    {
        if (services_ != nullptr)
        {
            services_->StartAll();
        }
    }
    void StopServices() noexcept
    {
        if (services_ != nullptr)
        {
            services_->StopAll();
        }
    }

  private:
    std::unique_ptr<ProvidedServicesBase> services_;
};

}  // namespace service
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_SERVICE_PROVIDED_SERVICE_CONTAINER_H
