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

#ifndef PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_TEST_ERROR_DOMAIN_H
#define PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_TEST_ERROR_DOMAIN_H

#include "score/result/error.h"
#include "score/result/result.h"

namespace bmw
{
  namespace mw
  {
    namespace com
    {
      namespace test
      {

        enum class TestErrorCode : bmw::result::ErrorCode
        {
          kCreateInstanceSpecifierFailed = 1,
          kCreateSkeletonFailed = 2,
        };

        class TestErrorDomain final : public bmw::result::ErrorDomain
        {
        public:
          amp::string_view MessageFor(const bmw::result::ErrorCode &code) const noexcept override;
        };

        bmw::result::Error MakeError(TestErrorCode code, std::string_view user_message = "") noexcept;

      } // namespace test
    } // namespace com
  } // namespace mw
} // namespace bmw

#endif // PLATFORM_AAS_MW_COM_TEST_COMMON_TEST_RESOURCES_TEST_ERROR_DOMAIN_H
