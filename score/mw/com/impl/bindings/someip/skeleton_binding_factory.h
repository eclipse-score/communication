/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_BINDING_FACTORY_H

#include "score/mw/com/impl/bindings/someip/deployment_resources.h"
#include "score/mw/com/impl/bindings/someip/skeleton.h"
#include "score/mw/com/impl/configuration/someip_service_instance_deployment.h"
#include "score/mw/com/impl/configuration/someip_service_type_deployment.h"

#include <memory>

namespace score::mw::com::impl::someip
{

/// \brief Creates the SOME/IP skeleton binding from the binding specific parts of the deployment.
///
/// The binding independent SkeletonBindingFactoryImpl only selects this factory via the deployment variant. All
/// knowledge about how a SOME/IP skeleton is constructed stays inside the binding.
struct SkeletonBindingFactory final
{
    static std::unique_ptr<Skeleton> Create(const SomeIpServiceInstanceDeployment& instance_deployment,
                                            const SomeIpServiceTypeDeployment& type_deployment) noexcept
    {
        return std::make_unique<Skeleton>(MakeServiceInstanceEndpoint(instance_deployment, type_deployment));
    }
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_BINDING_FACTORY_H
