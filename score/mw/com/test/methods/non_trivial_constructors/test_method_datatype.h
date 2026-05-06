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
#ifndef SCORE_MW_COM_TEST_METHODS_NON_TRIVIAL_CONSTRUCTORS_TEST_METHOD_DATATYPE_H
#define SCORE_MW_COM_TEST_METHODS_NON_TRIVIAL_CONSTRUCTORS_TEST_METHOD_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{

struct NonTriviallyConstructibleType
{
    NonTriviallyConstructibleType() : value_{kInitialValue} {}

    static constexpr std::int32_t kInitialValue = 21;

  private:
    friend NonTriviallyConstructibleType operator+(const NonTriviallyConstructibleType& a,
                                                   const NonTriviallyConstructibleType& b);
    friend bool operator==(const NonTriviallyConstructibleType& a, const NonTriviallyConstructibleType& b);
    friend std::ostream& operator<<(std::ostream& os, const NonTriviallyConstructibleType& obj);

    NonTriviallyConstructibleType(std::int32_t value) : value_{value} {}

    std::int32_t value_;
};

NonTriviallyConstructibleType operator+(const NonTriviallyConstructibleType& a, const NonTriviallyConstructibleType& b);
bool operator==(const NonTriviallyConstructibleType& a, const NonTriviallyConstructibleType& b);
bool operator!=(const NonTriviallyConstructibleType& a, const NonTriviallyConstructibleType& b);
std::ostream& operator<<(std::ostream& os, const NonTriviallyConstructibleType& obj);

/// \brief Test interface with methods covering all signature variations
/// \tparam T Either ProxyTrait or SkeletonTrait
template <typename T>
class NonTrivialConstructorInterface : public T::Base
{
  public:
    using T::Base::Base;

    /// \brief Method with both InArgs and Result
    typename T::template Method<NonTriviallyConstructibleType(NonTriviallyConstructibleType,
                                                              NonTriviallyConstructibleType)>
        with_in_args_and_return{*this, "with_in_args_and_return"};

    /// \brief Method with only InArgs, no Result (void return)
    typename T::template Method<void(NonTriviallyConstructibleType, NonTriviallyConstructibleType)> with_in_args_only{
        *this,
        "with_in_args_only"};

    /// \brief Method with only Result, no InArgs
    typename T::template Method<NonTriviallyConstructibleType()> with_return_only{*this, "with_return_only"};
};

/// \brief Proxy side of the test service
using NonTrivialConstructorProxy = score::mw::com::AsProxy<NonTrivialConstructorInterface>;

/// \brief Skeleton side of the test service
using NonTrivialConstructorSkeleton = score::mw::com::AsSkeleton<NonTrivialConstructorInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_NON_TRIVIAL_CONSTRUCTORS_TEST_METHOD_DATATYPE_H
