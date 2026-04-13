/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#ifndef SCORE_MW_COM_TEST_METHODS_SIGNATURE_VARIATIONS_TEST_METHOD_DATATYPE_H
#define SCORE_MW_COM_TEST_METHODS_SIGNATURE_VARIATIONS_TEST_METHOD_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{

/// \brief Test interface with methods covering all signature variations
/// \tparam T Either ProxyTrait or SkeletonTrait
template <typename T>
class MethodSignatureInterface : public T::Base
{
  public:
    using T::Base::Base;

    /// \brief Method with both InArgs and Result
    typename T::template Method<std::int32_t(std::int32_t, std::int32_t)> with_in_args_and_return{
        *this,
        "with_in_args_and_return"};

    /// \brief Method with only InArgs, no Result (void return)
    typename T::template Method<void(std::int32_t, std::int32_t)> with_in_args_only{*this, "with_in_args_only"};

    /// \brief Method with only Result, no InArgs
    typename T::template Method<std::int32_t()> with_return_only{*this, "with_return_only"};

    /// \brief Method without InArgs or Result
    typename T::template Method<void()> without_args_or_return{*this, "without_args_or_return"};
};

/// \brief Proxy side of the test service
using MethodSignatureProxy = score::mw::com::AsProxy<MethodSignatureInterface>;

/// \brief Skeleton side of the test service
using MethodSignatureSkeleton = score::mw::com::AsSkeleton<MethodSignatureInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_SIGNATURE_VARIATIONS_TEST_METHOD_DATATYPE_H
