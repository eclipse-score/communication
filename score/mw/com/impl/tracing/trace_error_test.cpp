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
#include "score/mw/com/impl/tracing/trace_error.h"

#include <include/gtest/gtest.h>

namespace score::mw::com::impl::tracing
{
namespace
{

class TraceConfigErrorTest : public ::testing::Test
{
  protected:
    void testErrorMessage(TraceErrorCode errorCode, std::string_view expectedErrorOutput)
    {
        const auto errorCodeTest =
            trace_config_error_domain_dummy.MessageFor(static_cast<score::result::ErrorCode>(errorCode));
        ASSERT_EQ(errorCodeTest, expectedErrorOutput);
    }

    TraceErrorDomain trace_config_error_domain_dummy{};
};

TEST_F(TraceConfigErrorTest, JsonConfigParseError)
{
    testErrorMessage(TraceErrorCode::JsonConfigParseError, "json config parsing error");
}

TEST_F(TraceConfigErrorTest, TraceErrorDisableAllTracePoints)
{
    testErrorMessage(TraceErrorCode::TraceErrorDisableAllTracePoints,
                     "Tracing is completely disabled because of unrecoverable error");
}

TEST_F(TraceConfigErrorTest, TraceErrorDisableTracePointInstance)
{
    testErrorMessage(TraceErrorCode::TraceErrorDisableTracePointInstance,
                     "Tracing for the given trace-point instance is disabled because of unrecoverable error");
}

TEST_F(TraceConfigErrorTest, MessageForDefault)
{
    testErrorMessage(static_cast<TraceErrorCode>(0), "unknown trace error");
}

}  // namespace
}  // namespace score::mw::com::impl::tracing
