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
#ifndef SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_TEST_METHOD_DATATYPE_H
#define SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_TEST_METHOD_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstdint>

namespace score::mw::com::test
{

/// \brief Test interface with multiple methods for selective enabling by different proxies
template <typename T>
class MultiMethodInterface : public T::Base
{
  public:
    using T::Base::Base;
    using ReturnType = std::int32_t;
    using InpArgType = std::int32_t;
    using MethodType = ReturnType(InpArgType);

    typename T::template Method<MethodType> method0{*this, "method0"};
    typename T::template Method<MethodType> method1{*this, "method1"};
    typename T::template Method<MethodType> method2{*this, "method2"};
    typename T::template Method<MethodType> method3{*this, "method3"};
    typename T::template Method<MethodType> method4{*this, "method4"};
};

/// \brief Proxy side of the test service
using MultiMethodProxy = score::mw::com::AsProxy<MultiMethodInterface>;

/// \brief Skeleton side of the test service
using MultiMethodSkeleton = score::mw::com::AsSkeleton<MultiMethodInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_METHODS_TEST_RESOURCES_TEST_METHOD_DATATYPE_H
