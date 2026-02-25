/*******************************************************************************
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
 *******************************************************************************/

#ifndef SCORE_MW_COM_TEST_FIELD_INITIAL_VALUE_TEST_DATATYPE_H
#define SCORE_MW_COM_TEST_FIELD_INITIAL_VALUE_TEST_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstdint>
#include <vector>

namespace score::mw::com::test
{

constexpr const char* const kInstanceSpecifierString = "test/field_initial_value";
const std::int32_t kTestValue = 18;

template <typename T>
class TestInterface : public T::Base
{
  public:
    using T::Base::Base;

    typename T::template Field<std::int32_t> test_field{*this, "test_field"};
};

using TestDataProxy = score::mw::com::AsProxy<TestInterface>;
using TestDataSkeleton = score::mw::com::AsSkeleton<TestInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_FIELD_RECEIVE_HANDLER_SRC_LOLA_H
