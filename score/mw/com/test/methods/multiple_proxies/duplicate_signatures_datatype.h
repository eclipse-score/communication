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
#ifndef SCORE_MW_COM_TEST_METHODS_MULTIPLE_PROXIES_DUPLICATE_SIGNATURES_DATATYPE_H
#define SCORE_MW_COM_TEST_METHODS_MULTIPLE_PROXIES_DUPLICATE_SIGNATURES_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstddef>
#include <cstdint>

namespace score::mw::com::test
{

/// \brief Test interface with multiple methods for selective enabling by different proxies
template <typename T>
class DuplicateSignatureInterface : public T::Base
{
  public:
    using T::Base::Base;

    using InputArgType = std::int32_t;
    using CallIndexType = std::size_t;
    using ConsumerIdType = std::size_t;
    using ProxyIdType = std::size_t;

    struct ReturnType
    {
        InputArgType sent_value;
        CallIndexType call_index;
        ConsumerIdType consumer_id;
        ProxyIdType proxy_id;
    };

    using MethodType = ReturnType(InputArgType, CallIndexType, ConsumerIdType, ProxyIdType);

    typename T::template Method<MethodType> method0{*this, "method0"};
    typename T::template Method<MethodType> method1{*this, "method1"};
    typename T::template Method<MethodType> method2{*this, "method2"};
    typename T::template Method<MethodType> method3{*this, "method3"};
    typename T::template Method<MethodType> method4{*this, "method4"};
};

/// \brief Proxy side of the test service
using DuplicateSignatureProxy = score::mw::com::AsProxy<DuplicateSignatureInterface>;

/// \brief Skeleton side of the test service
using DuplicateSignatureSkeleton = score::mw::com::AsSkeleton<DuplicateSignatureInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_METHODS_MULTIPLE_PROXIES_DUPLICATE_SIGNATURES_DATATYPE_H
