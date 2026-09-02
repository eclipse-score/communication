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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_H

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/bindings/someip/service_instance_endpoint.h"
#include "score/mw/com/impl/skeleton_binding.h"

#include <optional>
#include <utility>

namespace score::mw::com::impl::someip
{

/// \brief Skeleton side binding of the SOME/IP technical binding.
///
/// The instance is created by the binding independent SkeletonBindingFactoryImpl once the deployment of the service
/// instance selects the SOME/IP binding. It holds the endpoint the service instance is offered on.
class Skeleton final : public SkeletonBinding
{
  public:
    explicit Skeleton(ServiceInstanceEndpoint endpoint) noexcept : SkeletonBinding{}, endpoint_{std::move(endpoint)} {}

    ~Skeleton() noexcept override = default;

    const ServiceInstanceEndpoint& GetEndpoint() const noexcept
    {
        return endpoint_;
    }

    bool IsOffered() const noexcept
    {
        return is_offered_;
    }

    Result<void> PrepareOffer(SkeletonEventBindings&,
                              SkeletonFieldBindings&,
                              std::optional<RegisterShmObjectTraceCallback>) override
    {
        is_offered_ = true;
        return {};
    }

    void PrepareStopOffer(std::optional<UnregisterShmObjectTraceCallback>) override
    {
        is_offered_ = false;
    }

    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kSomeIp;
    }

    bool VerifyAllMethodHandlersRegistered() const override
    {
        return true;
    }

  private:
    ServiceInstanceEndpoint endpoint_;
    bool is_offered_{false};
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_H
