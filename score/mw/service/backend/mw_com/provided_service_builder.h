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

#ifndef SCORE_MW_SERVICE_BACKEND_MW_COM_PROVIDED_SERVICE_BUILDER_H
#define SCORE_MW_SERVICE_BACKEND_MW_COM_PROVIDED_SERVICE_BUILDER_H

#include "score/mw/service/backend/mw_com/provided_service_decorator.h"
#include "score/mw/service/provided_service_container.h"

#include <utility>

namespace score
{
namespace mw
{
namespace service
{

// Forward declare to allow alias
namespace backend::mw_com
{
template <typename ServiceType>
class ProvidedServiceDecorator;
}

namespace backend
{
namespace mw_com
{

/// @brief Minimal stub for config_daemon testing
class ProvidedServiceBuilder
{
  public:
    // Workaround: Since template aliases can't be used as template template parameters,
    // we expose ProvidedServiceDecorator through a type member that acts as a "forwarding" name
    // This allows: GetServices<ProvidedServiceBuilder::DecoratorType>()
    template <typename ServiceType>
    struct DecoratorType : ProvidedServiceDecorator<ServiceType>
    {
        using ProvidedServiceDecorator<ServiceType>::ProvidedServiceDecorator;
    };

    // Provide a convenient type alias for ProvidedServices instantiated with ProvidedServiceDecorator
    // This allows tests to use: ProvidedServiceBuilder::ProvidedServicesType
    using ProvidedServicesType = score::mw::service::ProvidedServices<ProvidedServiceDecorator>;

    ProvidedServiceBuilder() = default;

    template <typename ServiceType>
    ProvidedServiceBuilder& With(ServiceType&& service)
    {
        services_.Add<ServiceType>(std::forward<ServiceType>(service));
        return *this;
    }

    ProvidedServiceContainer GetServices()
    {
        return ProvidedServiceContainer{std::move(services_)};
    }

  private:
    score::mw::service::ProvidedServices<ProvidedServiceDecorator> services_;
};

// Backward compatibility: Tests expect mw::service::backend::mw_com::ProvidedServices
using ProvidedServices = score::mw::service::ProvidedServices<ProvidedServiceDecorator>;

}  // namespace mw_com
}  // namespace backend

// Make ProvidedServiceBuilder available in mw::service namespace
// for backward compatibility with existing code
using backend::mw_com::ProvidedServiceBuilder;

}  // namespace service
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_SERVICE_BACKEND_MW_COM_PROVIDED_SERVICE_BUILDER_H
