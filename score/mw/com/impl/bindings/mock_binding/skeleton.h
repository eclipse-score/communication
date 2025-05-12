/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#ifndef SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SKELETON_H
#define SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SKELETON_H

#include "score/mw/com/impl/skeleton_binding.h"

#include "gmock/gmock.h"

namespace score::mw::com::impl::mock_binding
{

class Skeleton : public SkeletonBinding
{
  public:
    using SkeletonBinding::SkeletonBinding;

    ~Skeleton() override = default;

    MOCK_METHOD(ResultBlank,
                PrepareOffer,
                (SkeletonEventBindings&, SkeletonFieldBindings&, std::optional<RegisterShmObjectTraceCallback>),
                (noexcept, override, final));
    MOCK_METHOD(void, PrepareStopOffer, (std::optional<UnregisterShmObjectTraceCallback>), (noexcept, override, final));
    MOCK_METHOD(BindingType, GetBindingType, (), (const, noexcept, override, final));
};

}  // namespace score::mw::com::impl::mock_binding

#endif  // SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SKELETON_H
