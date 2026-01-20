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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SKELETON_METHOD_H
#define SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SKELETON_METHOD_H

#include "score/mw/com/impl/methods/skeleton_method_binding.h"

#include <gmock/gmock.h>

namespace score::mw::com::impl::mock_binding
{

class SkeletonMethod : public SkeletonMethodBinding
{
  public:
    MOCK_METHOD(ResultBlank, RegisterHandler, (TypeErasedHandler&&), (override));
};

class SkeletonMethodFacade : public SkeletonMethodBinding
{
  public:
    SkeletonMethodFacade(SkeletonMethod& skeleton_method)
        : SkeletonMethodBinding(), skeleton_method_{skeleton_method} {};
    ~SkeletonMethodFacade() override = default;

    ResultBlank RegisterHandler(TypeErasedHandler&& cb) override
    {
        return skeleton_method_.RegisterHandler(std::move(cb));
    }

  private:
    SkeletonMethodBinding& skeleton_method_;
};

}  // namespace score::mw::com::impl::mock_binding

#endif  // SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SKELETON_METHOD_H
