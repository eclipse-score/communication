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
#ifndef SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_TEST_METHOD_DATATYPE_H
#define SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_TEST_METHOD_DATATYPE_H

#include "score/mw/com/impl/methods/proxy_method_with_in_args_and_return.h"
#include "score/mw/com/impl/methods/skeleton_method.h"
#include "score/mw/com/types.h"

#include <cstdint>
#include <vector>

namespace score::mw::com::test
{

template <typename Trait>
class TestMethodServiceInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    static const std::vector<const char*> method_names;

    // Method: takes two int32 arguments, returns int32
    typename Trait::template Method<std::int32_t(std::int32_t, std::int32_t)> test_method_{*this, method_names[0]};
};

template <typename Trait>
const std::vector<const char*> TestMethodServiceInterface<Trait>::method_names{"test_method"};

using TestMethodServiceProxy = AsProxy<TestMethodServiceInterface>;
using TestMethodServiceSkeleton = AsSkeleton<TestMethodServiceInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_PARTIAL_RESTART_METHODS_TEST_METHOD_DATATYPE_H
