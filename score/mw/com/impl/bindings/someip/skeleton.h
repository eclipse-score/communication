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
#include "score/mw/com/impl/skeleton_binding.h"

namespace score::mw::com::impl::someip
{

class Skeleton final : public SkeletonBinding
{
  public:
    Skeleton() = default;
    ~Skeleton() noexcept override = default;

    Result<void> PrepareOffer(SkeletonEventBindings&,
                              SkeletonFieldBindings&,
                              std::optional<RegisterShmObjectTraceCallback>) override
    {
        return {};
    }

    void PrepareStopOffer(std::optional<UnregisterShmObjectTraceCallback>) override {}

    BindingType GetBindingType() const noexcept override
    {
        return BindingType::kSomeIp;
    }

    bool VerifyAllMethodHandlersRegistered() const override
    {
        return true;
    }
};

}  // namespace score::mw::com::impl::someip

#endif  // SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_H
