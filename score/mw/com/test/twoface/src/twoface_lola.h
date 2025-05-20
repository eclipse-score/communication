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

#ifndef PLATFORM_AAS_MW_COM_TEST_TWOFACE_SRC_TWOFACE_LOLA_H
#define PLATFORM_AAS_MW_COM_TEST_TWOFACE_SRC_TWOFACE_LOLA_H

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

        namespace
        {

          constexpr const char *const kTwoFaceInstanceSpecifierString = "test/twoface";

        }

        template <typename T>
        class TestInterface : public T::Base
        {
        public:
          using T::Base::Base;

          typename T::template Event<std::int32_t> test_event{*this, "test_event"};
        };

        using TestDataProxy = bmw::mw::com::AsProxy<TestInterface>;
        using TestDataSkeleton = bmw::mw::com::AsSkeleton<TestInterface>;

      } // namespace test
    } // namespace com
  } // namespace mw
} // namespace bmw

#endif // PLATFORM_AAS_MW_COM_TEST_TWOFACE_SRC_TWOFACE_LOLA_H
