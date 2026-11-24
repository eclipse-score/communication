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

#include "score/mw/com/test/common_test_resources/stop_token_sig_term_handler.h"

#include <csignal>

#include <gtest/gtest.h>

namespace score::mw::com
{
namespace
{

class StopTokenSigTermHandlerTest : public ::testing::Test
{
  protected:
    void TearDown() override
    {
        // Restore default handlers
        std::signal(SIGTERM, SIG_DFL);
        std::signal(SIGINT, SIG_DFL);
    }
};

TEST_F(StopTokenSigTermHandlerTest, SetupSucceedsAndSigTermRequestsStop)
{
    score::cpp::stop_source stop_source{};
    const auto stop_token = stop_source.get_token();
    EXPECT_FALSE(stop_token.stop_requested());

    const bool setup_result = SetupStopTokenSigTermHandler(stop_source);
    EXPECT_TRUE(setup_result);

    std::raise(SIGTERM);

    EXPECT_TRUE(stop_token.stop_requested());
}

TEST_F(StopTokenSigTermHandlerTest, SetupSucceedsAndSigIntRequestsStop)
{
    score::cpp::stop_source stop_source{};
    const auto stop_token = stop_source.get_token();
    EXPECT_FALSE(stop_token.stop_requested());

    const bool setup_result = SetupStopTokenSigTermHandler(stop_source);
    EXPECT_TRUE(setup_result);

    std::raise(SIGINT);

    EXPECT_TRUE(stop_token.stop_requested());
}

}  // namespace
}  // namespace score::mw::com
