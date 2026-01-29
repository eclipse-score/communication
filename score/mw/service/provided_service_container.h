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

#include <cstddef>
#include <memory>

namespace score
{
namespace mw
{
namespace service
{
namespace backend
{
namespace mw_com
{
template <template <typename> class>
class ProvidedServices;
}
}  // namespace backend

/// @brief Minimal stub for config_daemon testing
class ProvidedServiceContainer
{
  public:
    ProvidedServiceContainer() = default;

    template <template <typename> class ServiceDecorator>
    explicit ProvidedServiceContainer(backend::mw_com::ProvidedServices<ServiceDecorator>&& services)
        : num_services_(services.size()),
          services_ptr_(std::make_unique<ServicesHolder<ServiceDecorator>>(std::move(services)))
    {
    }

    virtual ~ProvidedServiceContainer() = default;

    template <template <typename> class ServiceDecorator>
    const auto* GetServices() const noexcept
    {
        if (auto* holder = dynamic_cast<const ServicesHolder<ServiceDecorator>*>(services_ptr_.get()))
        {
            return &holder->services;
        }
        return static_cast<const backend::mw_com::ProvidedServices<ServiceDecorator>*>(nullptr);
    }

    template <template <typename> class ServiceDecorator>
    auto* GetServices() noexcept
    {
        if (auto* holder = dynamic_cast<ServicesHolder<ServiceDecorator>*>(services_ptr_.get()))
        {
            return &holder->services;
        }
        return static_cast<backend::mw_com::ProvidedServices<ServiceDecorator>*>(nullptr);
    }

    std::size_t NumServices() const noexcept
    {
        return num_services_;
    }

  private:
    struct ServicesHolderBase
    {
        virtual ~ServicesHolderBase() = default;
    };

    template <template <typename> class ServiceDecorator>
    struct ServicesHolder : ServicesHolderBase
    {
        backend::mw_com::ProvidedServices<ServiceDecorator> services;
        explicit ServicesHolder(backend::mw_com::ProvidedServices<ServiceDecorator>&& svc) : services(std::move(svc)) {}
    };

    std::size_t num_services_{0};
    std::unique_ptr<ServicesHolderBase> services_ptr_;
};

}  // namespace service
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_SERVICE_PROVIDED_SERVICE_CONTAINER_H
