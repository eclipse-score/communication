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
#include <gtest/gtest.h>

#include "score/message_passing/resource_manager_fixture_base.h"

namespace score::message_passing::detail
{
namespace
{

using namespace ::testing;

class DispatchThreadRunnerTestFixture : public ResourceManagerFixtureBase
{
  public:
    void SetUp() override
    {
        ResourceManagerFixtureBase::SetUp();
    }

    void TearDown() override
    {
        ResourceManagerFixtureBase::TearDown();
    }
};

using DispatchThreadRunnerDeathTest = DispatchThreadRunnerTestFixture;

TEST_F(DispatchThreadRunnerTestFixture, RunnerCreatedButNotStarted)
{
    // Given a DispatchThreadRunner instance
    DispatchThreadRunner runner(*channel_, *dispatch_, *signal_);

    // When Start() is not called

    // Then no activity is recorded by OSAL mocks
}

TEST_F(DispatchThreadRunnerDeathTest, RunnerStart_CreateChannel)
{
    // Times(AnyNumber()) to satisfy death test logic

    // Given a DispatchThreadRunner instance
    DispatchThreadRunner runner(*channel_, *dispatch_, *signal_);

    // Expecting a failing dispatch_create_channel call
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(kFakeOsError));

    // Then we terminate at Start() call
    EXPECT_DEATH(runner.Start([] {}), "Unable to allocate dispatch handle");
}

TEST_F(DispatchThreadRunnerDeathTest, RunnerStart_ConnectSideChannel)
{
    // Times(AnyNumber()) to satisfy death test logic

    // Given a DispatchThreadRunner instance
    DispatchThreadRunner runner(*channel_, *dispatch_, *signal_);

    const std::size_t index = kClientIndex;
    dispatch_t* const dispatch_ptr = reinterpret_cast<dispatch_t*>(&dispatches_[index]);

    // Expecting a failing message_connect call
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(dispatch_ptr));
    EXPECT_CALL(*dispatch_, message_connect).Times(AnyNumber()).WillOnce(Return(kFakeOsError));

    // Then we terminate at Start() call
    EXPECT_DEATH(runner.Start([] {}), "Unable to create side channel");
}

TEST_F(DispatchThreadRunnerDeathTest, RunnerStart_AttachQuitPulse)
{
    // Times(AnyNumber()) to satisfy death test logic

    // Given a DispatchThreadRunner instance
    DispatchThreadRunner runner(*channel_, *dispatch_, *signal_);

    const std::size_t index = kClientIndex;
    dispatch_t* const dispatch_ptr = reinterpret_cast<dispatch_t*>(&dispatches_[index]);
    const auto coid = static_cast<std::int32_t>(index);

    // Expecting a failing message_connect call
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(dispatch_ptr));
    EXPECT_CALL(*dispatch_, message_connect).Times(AnyNumber()).WillOnce(Return(coid));
    EXPECT_CALL(*dispatch_, pulse_attach(_, _, DispatchThreadRunner::kQuitPulseCode, _, _))
        .Times(AnyNumber())
        .WillOnce(Return(kFakeOsError));

    // Then we terminate at Start() call
    EXPECT_DEATH(runner.Start([] {}), "Unable to attach quit pulse code");
}

TEST_F(DispatchThreadRunnerDeathTest, RunnerStart_AllocateContext)
{
    // Times(AnyNumber()) to satisfy death test logic

    // Given a DispatchThreadRunner instance
    DispatchThreadRunner runner(*channel_, *dispatch_, *signal_);

    const std::size_t index = kClientIndex;
    ResourceManagerMockHelper& helper = helpers_[index];
    dispatch_t* const dispatch_ptr = reinterpret_cast<dispatch_t*>(&dispatches_[index]);
    const auto coid = static_cast<std::int32_t>(index);

    // Expecting a failing dispatch_context_alloc call
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(AnyNumber()).WillOnce(Return(dispatch_ptr));
    EXPECT_CALL(*dispatch_, message_connect).Times(AnyNumber()).WillOnce(Return(coid));
    EXPECT_CALL(*dispatch_, pulse_attach(_, _, DispatchThreadRunner::kQuitPulseCode, _, _))
        .Times(AnyNumber())
        .WillOnce(Invoke(&helper, &ResourceManagerMockHelper::pulse_attach));
    EXPECT_CALL(*dispatch_, dispatch_context_alloc).Times(AnyNumber()).WillOnce(Return(kFakeOsError));

    // Then we terminate at Start() call
    EXPECT_DEATH(runner.Start([] {}), "Unable to allocate context pointer");
}

TEST_F(DispatchThreadRunnerTestFixture, RunnerCreatedAndStarted)
{
    // Given a DispatchThreadRunner instance
    DispatchThreadRunner runner(*channel_, *dispatch_, *signal_);

    const std::size_t index = kClientIndex;
    ResourceManagerMockHelper& helper = helpers_[index];
    dispatch_t* const dispatch_ptr = reinterpret_cast<dispatch_t*>(&dispatches_[index]);
    dispatch_context_t* const context_ptr = &helper.context_;
    const auto coid = static_cast<std::int32_t>(index);

    // Expecting a set of OSAL calls
    EXPECT_CALL(*dispatch_, dispatch_create_channel).Times(1).WillOnce(Return(dispatch_ptr));
    EXPECT_CALL(*dispatch_, message_connect).Times(1).WillOnce(Return(coid));
    EXPECT_CALL(*dispatch_, pulse_attach(_, _, DispatchThreadRunner::kQuitPulseCode, _, _))
        .Times(1)
        .WillOnce(Invoke(&helper, &ResourceManagerMockHelper::pulse_attach));

    EXPECT_CALL(*dispatch_, dispatch_context_alloc).Times(1).WillOnce(Return(context_ptr));
    EXPECT_CALL(*signal_, SigEmptySet).Times(1);
    EXPECT_CALL(*signal_, AddTerminationSignal).Times(1);
    EXPECT_CALL(*signal_, PthreadSigMask(_, _, _)).Times(1);
    EXPECT_CALL(*signal_, PthreadSigMask(_, _)).Times(1);

    EXPECT_CALL(*dispatch_, dispatch_block)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper, &ResourceManagerMockHelper::dispatch_block));
    EXPECT_CALL(*dispatch_, dispatch_handler)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper, &ResourceManagerMockHelper::dispatch_handler));
    EXPECT_CALL(*channel_, MsgSendPulse)
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(&helper, &ResourceManagerMockHelper::MsgSendPulse));

    // When we call Start()
    runner.Start([] {});

    EXPECT_CALL(*channel_, ConnectDetach).Times(1);
    EXPECT_CALL(*dispatch_, dispatch_destroy).Times(1);
    EXPECT_CALL(*dispatch_, dispatch_context_free).Times(1);
    // And another set of OSAL calls when we destroy the runner
}

}  // namespace
}  // namespace score::message_passing::detail
