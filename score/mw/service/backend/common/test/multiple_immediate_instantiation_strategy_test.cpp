/*********************************************************************************
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

#include "score/mw/service/backend/common/test/multiple_immediate_instantiation_strategy.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

namespace score::mw::service::test
{
namespace
{

using namespace ::testing;

class DummyReceiver
{
  public:
    virtual ~DummyReceiver() = default;
};

class DummyReceiverImpl : public DummyReceiver
{
  public:
};

/// \brief Factory which is used by a FindServiceStrategy to instantiate a single DummyReceiverImpl
///
/// The mock receiver must be provided by the user.
class IsActiveReceiverMockFactory
{
  public:
    std::unique_ptr<DummyReceiver> operator()()
    {
        return std::make_unique<DummyReceiverImpl>();
    }
};

/// \brief Factory holding an invocation counter, provided via its constructor.
class RecordingReceiverFactory
{
  public:
    explicit RecordingReceiverFactory(std::size_t& invocation_count) : invocation_count_{&invocation_count} {}

    std::unique_ptr<DummyReceiver> operator()()
    {
        ++(*invocation_count_);
        return std::make_unique<DummyReceiverImpl>();
    }

  private:
    std::size_t* invocation_count_;
};

class DestructorRecorder
{
  public:
    DestructorRecorder(bool& was_destructor_called) : was_destructor_called_{&was_destructor_called} {}

    ~DestructorRecorder()
    {
        if (was_destructor_called_ != nullptr)
        {
            *was_destructor_called_ = true;
        }
    }

    DestructorRecorder(const DestructorRecorder&) = delete;
    DestructorRecorder& operator=(const DestructorRecorder&) = delete;
    DestructorRecorder(DestructorRecorder&& other) noexcept : was_destructor_called_(other.was_destructor_called_)
    {
        other.was_destructor_called_ = nullptr;
    }
    DestructorRecorder& operator=(DestructorRecorder&&) noexcept = delete;

  private:
    bool* was_destructor_called_;
};

class MultipleImmediateInstantiationStrategyFixture : public ::testing::Test
{
  public:
    std::unique_ptr<typename ProxyBuilderBase<Multiple<DummyReceiver>>::BuilderCallback>
    CreateBuilderCallbackWithMockedUserCallback()
    {
        score::concurrency::InterruptiblePromise<MultiInstanceHolder<DummyReceiver>> promise{};
        score::cpp::callback<void(DummyReceiver&)> user_callback = [this](DummyReceiver& dummy_receiver) {
            mock_user_callback.AsStdFunction()(dummy_receiver);
        };
        auto on_found_builder_callback =
            std::make_unique<typename ProxyBuilderBase<Multiple<DummyReceiver>>::BuilderCallback>(
                std::move(promise), std::move(user_callback));
        return on_found_builder_callback;
    }

    MockFunction<void(DummyReceiver&)> mock_user_callback{};
};

TEST_F(MultipleImmediateInstantiationStrategyFixture, CallingFindWithNumberOfFoundProxiesZeroDoesNotInvokeCallback)
{
    // Given a MultipleImmediateInstantiationStrategy which has NumberOfFoundProxies set to 0
    constexpr std::size_t kNumberOfFoundProxies{0U};
    test::MultipleImmediateInstantiationStrategy<DummyReceiver, IsActiveReceiverMockFactory, kNumberOfFoundProxies>
        strategy{};

    // Expecting that the user callback will not be called
    EXPECT_CALL(mock_user_callback, Call(_)).Times(0);

    // When calling Find() with a BuilderCallback which wraps mocked user callback
    auto on_found_builder_callback = CreateBuilderCallbackWithMockedUserCallback();
    strategy.Find(std::move(on_found_builder_callback));
}

TEST_F(MultipleImmediateInstantiationStrategyFixture, CallingFindWithNumberOfFoundProxiesOneInvokesCallbackOnce)
{
    // Given a MultipleImmediateInstantiationStrategy which has NumberOfFoundProxies set to 1
    constexpr std::size_t kNumberOfFoundProxies{1U};
    test::MultipleImmediateInstantiationStrategy<DummyReceiver, IsActiveReceiverMockFactory, kNumberOfFoundProxies>
        strategy{};

    // Expecting that the user callback will be called once
    EXPECT_CALL(mock_user_callback, Call(_)).Times(1);

    // When calling Find() with a BuilderCallback which wraps mocked user callback
    auto on_found_builder_callback = CreateBuilderCallbackWithMockedUserCallback();
    strategy.Find(std::move(on_found_builder_callback));
}

TEST_F(MultipleImmediateInstantiationStrategyFixture,
       CallingFindWithNumberOfFoundProxiesMultipleInvokesCallbackMultipleTimes)
{
    // Given a MultipleImmediateInstantiationStrategy which has NumberOfFoundProxies set to 3
    constexpr std::size_t kNumberOfFoundProxies{3U};
    test::MultipleImmediateInstantiationStrategy<DummyReceiver, IsActiveReceiverMockFactory, kNumberOfFoundProxies>
        strategy{};

    // Expecting that the user callback will be called 3 times
    EXPECT_CALL(mock_user_callback, Call(_)).Times(3);

    // When calling Find() with a BuilderCallback which wraps mocked user callback
    auto on_found_builder_callback = CreateBuilderCallbackWithMockedUserCallback();
    strategy.Find(std::move(on_found_builder_callback));
}

TEST_F(MultipleImmediateInstantiationStrategyFixture, ConstructingWithFactoryInstanceInvokesProvidedFactoryOnFind)
{
    // Given a MultipleImmediateInstantiationStrategy constructed with a RecordingReceiverFactory instance
    constexpr std::size_t kNumberOfFoundProxies{3U};
    std::size_t invocation_count{0U};
    test::MultipleImmediateInstantiationStrategy<DummyReceiver, RecordingReceiverFactory, kNumberOfFoundProxies>
        strategy{RecordingReceiverFactory{invocation_count}};

    // When calling Find() with a BuilderCallback which wraps mocked user callback
    auto on_found_builder_callback = CreateBuilderCallbackWithMockedUserCallback();
    strategy.Find(std::move(on_found_builder_callback));

    // Then the factory instance provided via the constructor should have been invoked 3 times
    EXPECT_EQ(invocation_count, kNumberOfFoundProxies);
}

}  // namespace
}  // namespace score::mw::service::test
