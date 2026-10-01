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

namespace score::mw::service
{

// Forward declare to allow alias
namespace backend::mw_com
{
template <typename ServiceType>
class ProvidedServiceDecorator;
}

namespace backend::mw_com
{

/// @brief Minimal stub for config_daemon testing
class ProvidedServiceBuilder
{
  public:
    // Type alias so callers can use: GetServices<ProvidedServiceBuilder::DecoratorType>()
    // This must be a pass-through alias to ProvidedServiceDecorator (not a distinct subclass),
    // otherwise ProvidedServices<DecoratorType> and ProvidedServices<ProvidedServiceDecorator>
    // would be unrelated types and dynamic_cast-based lookups would always fail.
    template <typename ServiceType>
    using DecoratorType = ProvidedServiceDecorator<ServiceType>;

    // Provide a convenient type alias for ProvidedServices instantiated with DecoratorType
    // This allows tests to use: ProvidedServiceBuilder::ProvidedServicesType
    using ProvidedServicesType = score::mw::service::ProvidedServices<DecoratorType>;

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
    score::mw::service::ProvidedServices<DecoratorType> services_;
};

// Backward compatibility: Tests expect mw::service::backend::mw_com::ProvidedServices
// Must use ProvidedServiceBuilder::DecoratorType (not ProvidedServiceDecorator directly) so that
// this alias is the SAME template-template-argument instantiation as used internally by
// ProvidedServiceBuilder/GetServices<>() -- some compilers treat a pass-through alias template
// as a distinct template-template-parameter from the template it aliases.
using ProvidedServices = score::mw::service::ProvidedServices<ProvidedServiceBuilder::DecoratorType>;

}  // namespace backend::mw_com

// Make ProvidedServiceBuilder available in mw::service namespace
// for backward compatibility with existing code
using backend::mw_com::ProvidedServiceBuilder;

}  // namespace score::mw::service

#endif  // SCORE_MW_SERVICE_BACKEND_MW_COM_PROVIDED_SERVICE_BUILDER_H
