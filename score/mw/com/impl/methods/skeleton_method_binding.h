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
#ifndef SCORE_MW_COM_IMPL_METHODS_SKELETON_METHOD_BINDING_H
#define SCORE_MW_COM_IMPL_METHODS_SKELETON_METHOD_BINDING_H

#include "score/result/result.h"

#include <score/callback.hpp>
#include <score/span.hpp>

#include <algorithm>
#include <memory>
#include <optional>

namespace score::mw::com::impl
{

class SkeletonMethodBinding
{
  public:
    using TypeErasedCallbackSignature = void(std::optional<score::cpp::span<std::byte>>, std::optional<score::cpp::span<std::byte>>);
    // size of storred callback should be the base size of amp callback and a unique_ptr
    // this way the user can pass any information to the callback through the pointer.
    //
    // False positive: This value is used as part of the type TypeErasedHandler.
    // coverity[autosar_cpp14_a0_1_1_violation: FALSE]
    static constexpr std::size_t CallbackSize{
        sizeof(score::cpp::callback<TypeErasedCallbackSignature>) +
        std::max(score::cpp::callback<TypeErasedCallbackSignature>::alignment_t::value, sizeof(std::unique_ptr<void>))};

    using TypeErasedHandler = score::cpp::callback<TypeErasedCallbackSignature, CallbackSize>;

    SkeletonMethodBinding() = default;
    virtual ~SkeletonMethodBinding() = default;

    virtual ResultBlank RegisterHandler(TypeErasedHandler&& callback) = 0;

    SkeletonMethodBinding(const SkeletonMethodBinding&) = delete;
    SkeletonMethodBinding& operator=(const SkeletonMethodBinding&) & = delete;
    SkeletonMethodBinding(SkeletonMethodBinding&&) noexcept = delete;
    SkeletonMethodBinding& operator=(SkeletonMethodBinding&&) & noexcept = delete;
};

}  // namespace score::mw::com::impl
#endif  // SCORE_MW_COM_IMPL_METHODS_SKELETON_METHOD_BINDING_H
