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

#include "score/mw/com/test/common_test_resources/test_error_domain.h"

std::string_view score::mw::com::test::TestErrorDomain::MessageFor(const score::result::ErrorCode& code) const noexcept
{
    switch (static_cast<score::mw::com::test::TestErrorCode>(code))
    {
        case score::mw::com::test::TestErrorCode::kCreateInstanceSpecifierFailed:
            return "Failed to create instance specifier.";
        case score::mw::com::test::TestErrorCode::kCreateSkeletonFailed:
            return "Failed to create skeleton.";
        default:
            return "Unknown Error!";
    }
}

constexpr score::mw::com::test::TestErrorDomain test_error_domain;
score::result::Error score::mw::com::test::MakeError(score::mw::com::test::TestErrorCode code,
                                                 std::string_view user_message) noexcept
{
    return {static_cast<score::result::ErrorCode>(code), test_error_domain, user_message};
}
