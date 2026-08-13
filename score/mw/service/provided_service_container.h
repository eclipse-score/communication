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

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

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

    ProvidedServices& operator=(ProvidedServices&& other) & noexcept
    {
        if (this != &other)
        {
            StopAll();
            services_ = std::move(other.services_);
        }
        return *this;
    }
    constexpr ProvidedServices& operator=(const ProvidedServices&) & = delete;
    constexpr ProvidedServices(ProvidedServices&&) noexcept = default;
    constexpr ProvidedServices(const ProvidedServices&) = delete;

    /// @brief Add a new service instance by constructing it in-place via the provided arguments
    template <typename ServiceType, typename... Args>
    ProvidedServices& Add(Args&&... args) &
    {
        return AddViaInstanceSpecifier<ServiceType>(InstanceSpecifierView{}, std::forward<Args>(args)...);
    }

    template <typename ServiceType, typename... Args>
    ProvidedServices&& Add(Args&&... args) &&
    {
        std::ignore =
            this->AddViaInstanceSpecifier<ServiceType>(InstanceSpecifierView{}, std::forward<Args>(args)...);
        return std::move(*this);
    }

    template <typename ServiceType, typename... Args>
    ProvidedServices& AddViaInstanceSpecifier(InstanceSpecifierView instance_specifier, Args&&... args) &
    {
        return EmplaceServiceInstance<ServiceType>(
            instance_specifier, std::in_place_type<ServiceType>, std::forward<Args>(args)...);
    }

    template <typename ServiceType, typename... Args>
    ProvidedServices&& AddViaInstanceSpecifier(InstanceSpecifierView instance_specifier, Args&&... args) &&
    {
        std::ignore = this->EmplaceServiceInstance<ServiceType>(
            instance_specifier, std::in_place_type<ServiceType>, std::forward<Args>(args)...);
        return std::move(*this);
    }

    template <typename ServiceBaseType, typename ServiceImplType, typename... Args>
    ProvidedServices& EmplaceServiceInstance(std::in_place_type_t<ServiceImplType>, Args&&... args) &
    {
        return this->template EmplaceServiceInstance<ServiceBaseType>(
            InstanceSpecifierView{}, std::in_place_type<ServiceImplType>, std::forward<Args>(args)...);
    }

    template <typename ServiceBaseType, typename ServiceImplType, typename... Args>
    ProvidedServices&& EmplaceServiceInstance(std::in_place_type_t<ServiceImplType>, Args&&... args) &&
    {
        return std::move(this->template EmplaceServiceInstance<ServiceBaseType>(
            std::in_place_type<ServiceImplType>, std::forward<Args>(args)...));
    }

    template <typename ServiceBaseType, typename ServiceImplType, typename... Args>
    ProvidedServices& EmplaceServiceInstance(InstanceSpecifierView instance_specifier,
                                             std::in_place_type_t<ServiceImplType>,
                                             Args&&... args)
    {
        static_assert(std::is_base_of_v<ServiceBaseType, ServiceImplType>,
                      "Specified ServiceImplType must inherit from specified ServiceBaseType");

        if constexpr (std::is_base_of_v<ProvidedService, ServiceBaseType>)
        {
            return EmplaceUndecorated<ServiceImplType>(instance_specifier, std::forward<Args>(args)...);
        }
        else
        {
            return EmplaceDecorated<ServiceBaseType, ServiceImplType>(
                instance_specifier, std::in_place_type<ServiceImplType>, std::forward<Args>(args)...);
        }
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
        return ExtractImpl<ServiceType>([instance_specifier](const auto& service_element) noexcept -> bool {
            return instance_specifier.empty() ||
                   std::get<InstanceSpecifierType>(service_element) == instance_specifier;
        });
    }

    template <typename ServiceType>
    const ServiceType* Get() const noexcept
    {
        return Get<ServiceType>(InstanceSpecifierView{});
    }
    template <typename ServiceType>
    ServiceType* Get() noexcept
    {
        return Get<ServiceType>(InstanceSpecifierView{});
    }

    template <typename ServiceType>
    const ServiceType* Get(InstanceSpecifierView instance_specifier) const noexcept
    {
        return GetImpl<ServiceType>([instance_specifier](const auto& service_element) noexcept -> bool {
            return instance_specifier.empty() ||
                   std::get<InstanceSpecifierType>(service_element) == instance_specifier;
        });
    }

    template <typename ServiceType>
    ServiceType* Get(InstanceSpecifierView instance_specifier) noexcept
    {
        return GetImpl<ServiceType>([instance_specifier](const auto& service_element) noexcept -> bool {
            return instance_specifier.empty() ||
                   std::get<InstanceSpecifierType>(service_element) == instance_specifier;
        });
    }

    template <typename ServiceType>
    bool Has() const noexcept
    {
        return Has<ServiceType>(InstanceSpecifierView{});
    }

    bool Has(InstanceSpecifierView instance_specifier) const noexcept
    {
        return std::find_if(services_.cbegin(),
                            services_.cend(),
                            [instance_specifier](const auto& service_element) noexcept -> bool {
                                return std::get<InstanceSpecifierType>(service_element) == instance_specifier;
                            }) != services_.cend();
    }

    template <typename ServiceType>
    bool Has(InstanceSpecifierView instance_specifier) const noexcept
    {
        return std::find_if(services_.cbegin(),
                            services_.cend(),
                            [instance_specifier](const auto& service_element) noexcept -> bool {
                                const auto& current_instance_specifier =
                                    std::get<InstanceSpecifierType>(service_element);
                                const auto& service_holder = std::get<ProvidedServiceHolder>(service_element);
                                const bool instance_specifier_matches =
                                    instance_specifier.empty() || current_instance_specifier == instance_specifier;
                                return ((instance_specifier_matches && service_holder != nullptr) &&
                                        DynamicCast<ServiceType>(service_holder.get()) != nullptr);
                            }) != services_.cend();
    }

    std::size_t Count() const noexcept override
    {
        return static_cast<std::size_t>(
            std::count_if(services_.cbegin(), services_.cend(), [](const auto& service_element) noexcept -> bool {
                return std::get<ProvidedServiceHolder>(service_element) != nullptr;
            }));
    }

    /// @brief Start all contained valid service instances
    void StartAll() override
    {
        for (auto& service_element : services_)
        {
            auto& service = std::get<ProvidedServiceHolder>(service_element);
            if (service != nullptr)
            {
                service->StartService();
            }
        }
    }

    /// @brief Stop all contained valid service instances
    void StopAll() override
    {
        for (auto& service_element : services_)
        {
            auto& service = std::get<ProvidedServiceHolder>(service_element);
            if (service != nullptr)
            {
                service->StopService();
            }
        }
    }

    /// @brief Swap the content of this object with another one
    void Swap(ProvidedServices& other) noexcept
    {
        std::swap(services_, other.services_);
    }

  private:
    template <typename ServiceType, typename Predicate>
    auto ExtractImpl(Predicate predicate) noexcept
    {
        using ResultType = decltype(DynamicExtract<ServiceType>(std::declval<ProvidedServiceHolder&>()));

        for (auto it = services_.begin(); it != services_.end(); ++it)
        {
            if (std::invoke(predicate, *it))
            {
                ResultType extracted_service = DynamicExtract<ServiceType>(std::get<ProvidedServiceHolder>(*it));
                if (extracted_service != nullptr)
                {
                    services_.erase(it);
                    return extracted_service;
                }
            }
        }
        return ResultType{};
    }

    template <typename ServiceType, typename Predicate>
    ServiceType* GetImpl(Predicate predicate) const noexcept
    {
        for (const auto& service_element : services_)
        {
            if (std::invoke(predicate, service_element))
            {
                auto* found_service =
                    DynamicCast<ServiceType>(std::get<ProvidedServiceHolder>(service_element).get());
                if (found_service != nullptr)
                {
                    return found_service;
                }
            }
        }
        return nullptr;
    }

    template <typename ServiceType>
    static auto DynamicExtract(ProvidedServiceHolder& provided_service) noexcept
    {
        if constexpr (std::is_base_of_v<ProvidedService, ServiceType>)
        {
            std::unique_ptr<ServiceType> extracted_service;
            if (auto* const found_service = dynamic_cast<ServiceType*>(provided_service.get());
                found_service != nullptr)
            {
                std::ignore = provided_service.release();
                extracted_service = decltype(extracted_service){found_service};
            }
            return extracted_service;
        }
        else
        {
            typename ServiceDecorator<ServiceType>::ServiceHolder extracted_service{};
            if (auto* const found_service = dynamic_cast<ServiceDecorator<ServiceType>*>(provided_service.get());
                found_service != nullptr)
            {
                extracted_service = found_service->ExtractService();
                provided_service = ProvidedServiceHolder{};
            }
            return extracted_service;
        }
    }

    /// @brief Dynamic cast helper to safely cast a stored ProvidedService to a specific ServiceType.
    /// @details For "undecorated" services (which directly inherit from ProvidedService), a plain
    ///          dynamic_cast is used. For all other ("decorated") services, the stored object is first
    ///          cast to the corresponding ServiceDecorator<ServiceType> before extracting the wrapped
    ///          service via GetService().
    template <typename ServiceType>
    static ServiceType* DynamicCast(ProvidedService* service) noexcept
    {
        if constexpr (std::is_base_of_v<ProvidedService, ServiceType>)
        {
            return dynamic_cast<ServiceType*>(service);
        }
        else
        {
            if (auto* const found_service = dynamic_cast<ServiceDecorator<ServiceType>*>(service);
                found_service != nullptr)
            {
                return found_service->GetService();
            }
            return nullptr;
        }
    }

    template <typename ServiceBaseType, typename ServiceImplType, typename... Args>
    ProvidedServices& EmplaceDecorated(InstanceSpecifierView instance_specifier,
                                       std::in_place_type_t<ServiceImplType>,
                                       Args&&... args)
    {
        using DecoratorType = ServiceDecorator<ServiceBaseType>;
        services_.emplace_back(InstanceSpecifierType{instance_specifier},
                               std::make_unique<DecoratorType>(
                                   DecoratorType::template Create<ServiceImplType>(std::forward<Args>(args)...)));
        return *this;
    }

    template <typename ServiceType, typename... Args>
    ProvidedServices& EmplaceUndecorated(InstanceSpecifierView instance_specifier, Args&&... args)
    {
        static_assert(std::is_base_of_v<ProvidedService, ServiceType>,
                      "Specified ServiceType must inherit from mw::service::ProvidedService");
        services_.emplace_back(InstanceSpecifierType{instance_specifier},
                               std::make_unique<ServiceType>(std::forward<Args>(args)...));
        return *this;
    }

    std::vector<std::pair<InstanceSpecifierType, ProvidedServiceHolder>> services_;
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
