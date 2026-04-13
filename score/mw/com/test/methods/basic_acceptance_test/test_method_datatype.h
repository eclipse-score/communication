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
#ifndef SCORE_MW_COM_TEST_METHODS_BASIC_ACCEPTANCE_TEST_TEST_METHOD_DATATYPE_H
#define SCORE_MW_COM_TEST_METHODS_BASIC_ACCEPTANCE_TEST_TEST_METHOD_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{

/// \brief Test interface template following the mw::com traits pattern
/// \tparam T Either ProxyTrait or SkeletonTrait
template <typename T>
class TestMethodInterface : public T::Base
{
  public:
    using T::Base::Base;

    /// \brief Single method to validate interprocess method calls
    typename T::template Method<std::int32_t(std::int32_t, std::int32_t)> with_in_args_and_return{
        *this,
        "with_in_args_and_return"};
};

/// \brief Proxy side of the test service
using TestMethodProxy = score::mw::com::AsProxy<TestMethodInterface>;

/// \brief Skeleton side of the test service
using TestMethodSkeleton = score::mw::com::AsSkeleton<TestMethodInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_BASIC_ACCEPTANCE_TEST_TEST_METHOD_DATATYPE_H
