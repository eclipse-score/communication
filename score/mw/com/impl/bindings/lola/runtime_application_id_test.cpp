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
#include "score/mw/com/impl/bindings/lola/runtime.h"

#include "score/concurrency/thread_pool.h"
#include "score/os/mocklib/unistdmock.h"
#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/configuration/global_configuration.h"

#include <gtest/gtest.h>
#include <memory>

using namespace score::mw::com::impl;
using namespace score::mw::com::impl::lola;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

class LolaRuntimeApplicationIdTest : public ::testing::Test
{
  protected:
    LolaRuntimeApplicationIdTest()
    {
        // Set a default action for OS calls that happen during lola::Runtime
        // construction but are not relevant to the logic being tested here.
        // This avoids GMock warnings about uninteresting calls.
        ON_CALL(*unistd_mock_guard_, getpid()).WillByDefault(Return(12345));
        ON_CALL(*unistd_mock_guard_, readlink(_, _, _)).WillByDefault(Return(-1));
    }

    score::concurrency::ThreadPool executor_{1U};
    score::os::MockGuard<NiceMock<score::os::UnistdMock>> unistd_mock_guard_{};
};

TEST_F(LolaRuntimeApplicationIdTest, GetApplicationIdUsesConfiguredValueWhenPresent)
{
    // Given a configuration with an explicit applicationID
    const std::uint32_t configured_id = 12345;

    GlobalConfiguration global_config;
    global_config.SetApplicationId(configured_id);
    Configuration config({}, {}, std::move(global_config), {});

    // When the LoLa Runtime is constructed
    Runtime lola_runtime(config, executor_, nullptr);

    // Then the configured applicationID is used
    EXPECT_EQ(lola_runtime.GetApplicationId(), configured_id);
}

TEST_F(LolaRuntimeApplicationIdTest, GetApplicationIdFallsBackToProcessUidWhenNotConfigured)
{
    // Given a configuration without an explicit applicationID
    const uid_t process_uid = 999;  // This remains uid_t as it mocks the OS call
    ON_CALL(*unistd_mock_guard_, getuid()).WillByDefault(Return(process_uid));
    Configuration config({}, {}, {}, {});

    // When the LoLa Runtime is constructed
    Runtime lola_runtime(config, executor_, nullptr);

    // Then the process UID is used as a fallback
    EXPECT_EQ(lola_runtime.GetApplicationId(), static_cast<std::uint32_t>(process_uid));
}

TEST_F(LolaRuntimeApplicationIdTest, GetApplicationIdHandlesZeroValue)
{
    // Given a configuration with an applicationID of 0
    const std::uint32_t configured_id = 0;
    GlobalConfiguration global_config;
    global_config.SetApplicationId(configured_id);
    Configuration config({}, {}, std::move(global_config), {});

    // When the LoLa Runtime is constructed
    Runtime lola_runtime(config, executor_, nullptr);

    // Then the applicationID is correctly set to 0
    EXPECT_EQ(lola_runtime.GetApplicationId(), configured_id);
}

TEST_F(LolaRuntimeApplicationIdTest, GetApplicationIdHandlesMaxValue)
{
    // Given a configuration with the maximum uint32_t value.
    // This also covers the case where a negative value like -1 is
    // provided in the JSON config, which is cast to UINT32_MAX.
    const std::uint32_t configured_id = std::numeric_limits<std::uint32_t>::max();
    GlobalConfiguration global_config;
    global_config.SetApplicationId(configured_id);
    Configuration config({}, {}, std::move(global_config), {});

    // When the LoLa Runtime is constructed
    Runtime lola_runtime(config, executor_, nullptr);

    // Then the applicationID is correctly set to the maximum value
    EXPECT_EQ(lola_runtime.GetApplicationId(), configured_id);
}
