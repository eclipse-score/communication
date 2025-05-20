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

#ifndef PLATFORM_AAS_MW_COM_TEST_FIELD_INITIAL_VALUE_TEST_DATATYPE_H
#define PLATFORM_AAS_MW_COM_TEST_FIELD_INITIAL_VALUE_TEST_DATATYPE_H

#include "score/mw/com/types.h"

#include <cstdint>
#include <vector>

namespace bmw
{
  namespace mw
  {
    namespace com
    {
      namespace test
      {

        constexpr const char *const kInstanceSpecifierStringClient = "test/field_initial_value_client";
        constexpr const char *const kInstanceSpecifierStringServiceFirst = "test/field_initial_value_service_first";
        constexpr const char *const kInstanceSpecifierStringServiceSecond = "test/field_initial_value_service_second";
        const std::int32_t kNumberOfOfferedServices = 2;
        const std::int32_t kTestValue = 986;

        template <typename T>
        class TestInterface : public T::Base
        {
        public:
          using T::Base::Base;

          typename T::template Field<std::int32_t> test_field{*this, "test_field"};
        };

        using TestDataProxy = bmw::mw::com::AsProxy<TestInterface>;
        using TestDataSkeleton = bmw::mw::com::AsSkeleton<TestInterface>;

      } // namespace test
    } // namespace com
  } // namespace mw
} // namespace bmw

#endif // PLATFORM_AAS_MW_COM_TEST_FIELD_RECEIVE_HANDLER_SRC_LOLA_H
