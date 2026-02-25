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

#ifndef SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_TEST_ERROR_DOMAIN_H
#define SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_TEST_ERROR_DOMAIN_H

#include "score/result/error.h"
#include "score/result/result.h"

namespace score::mw::com::test
{

enum class TestErrorCode : score::result::ErrorCode
{
    kCreateInstanceSpecifierFailed = 1,
    kCreateSkeletonFailed = 2,
};

class TestErrorDomain final : public score::result::ErrorDomain
{
  public:
    std::string_view MessageFor(const score::result::ErrorCode& code) const noexcept override;
};

score::result::Error MakeError(TestErrorCode code, std::string_view user_message = "") noexcept;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_TEST_COMMON_TEST_RESOURCES_TEST_ERROR_DOMAIN_H
