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
#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/configuration/global_configuration.h"
#include "score/os/mocklib/unistdmock.h"

#include <gtest/gtest.h>
#include <memory>

using namespace score::mw::com::impl;
using namespace score::mw::com::impl::lola;
using ::testing::Return;
using ::testing::ReturnRef;

class LolaRuntimeApplicationIdTest : public ::testing::Test
{
  protected:
    score::concurrency::ThreadPool executor_{1U};
    score::os::MockGuard<score::os::UnistdMock> unistd_mock_guard_{};
};

TEST_F(LolaRuntimeApplicationIdTest, GetApplicationIdUsesConfiguredValueWhenPresent)
{
    // Given a configuration with an explicit applicationID
    const uid_t configured_id = 12345;
    const uid_t process_uid = 999;
    EXPECT_CALL(*unistd_mock_guard_, getuid()).WillOnce(Return(process_uid));

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
    const uid_t process_uid = 999;
    EXPECT_CALL(*unistd_mock_guard_, getuid()).WillOnce(Return(process_uid));
    Configuration config({}, {}, {}, {});

    // When the LoLa Runtime is constructed
    Runtime lola_runtime(config, executor_, nullptr);

    // Then the process UID is used as a fallback
    EXPECT_EQ(lola_runtime.GetApplicationId(), process_uid);
}