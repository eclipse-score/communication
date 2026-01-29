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
#include "score/mw/service/backend/mw_com/provided_services.h"
#include "score/mw/service/provided_service_container.h"

#include <utility>

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

/// @brief Minimal stub for config_daemon testing
class ProvidedServiceBuilder
{
  public:
    using DecoratorType = ProvidedServiceDecorator;

    ProvidedServiceBuilder() = default;

    template <typename ServiceType>
    ProvidedServiceBuilder& With(ServiceType&& /*service*/)
    {
        return *this;
    }

    ProvidedServiceContainer GetServices()
    {
        return ProvidedServiceContainer{};
    }
};

}  // namespace mw_com
}  // namespace backend
}  // namespace service
}  // namespace mw
}  // namespace score

#endif  // SCORE_MW_SERVICE_BACKEND_MW_COM_PROVIDED_SERVICE_BUILDER_H
