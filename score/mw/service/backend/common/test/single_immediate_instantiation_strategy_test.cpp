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

#include "score/mw/service/backend/common/test/single_immediate_instantiation_strategy.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <functional>
#include <memory>
#include <utility>

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

/// \brief Factory holding state (whether it was invoked), provided via its constructor.
class RecordingReceiverFactory
{
  public:
    explicit RecordingReceiverFactory(bool& was_invoked) : was_invoked_{&was_invoked} {}

    std::unique_ptr<DummyReceiver> operator()()
    {
        *was_invoked_ = true;
        return std::make_unique<DummyReceiverImpl>();
    }

  private:
    bool* was_invoked_;
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

class SingleImmediateInstantiationStrategyFixture : public ::testing::Test
{
  public:
    std::unique_ptr<typename ProxyBuilderBase<DummyReceiver>::BuilderCallback>
    CreateBuilderCallbackWithMockedUserCallback()
    {
        score::concurrency::InterruptiblePromise<SingleInstanceHolder<DummyReceiver>> promise{};
        score::cpp::callback<void(DummyReceiver&)> user_callback = [this](DummyReceiver& dummy_receiver) {
            mock_user_callback.AsStdFunction()(dummy_receiver);
        };
        auto on_found_builder_callback = std::make_unique<typename ProxyBuilderBase<DummyReceiver>::BuilderCallback>(
            std::move(promise), std::move(user_callback));
        return on_found_builder_callback;
    }

    std::unique_ptr<typename ProxyBuilderBase<DummyReceiver>::BuilderCallback>
    CreateBuilderCallbackWithDestructorRecordingUserCallback()
    {
        score::concurrency::InterruptiblePromise<SingleInstanceHolder<DummyReceiver>> promise{};
        score::cpp::callback<void(DummyReceiver&)> user_callback =
            [destructor_recorder = DestructorRecorder{was_destructor_called}](DummyReceiver&) {};
        auto on_found_builder_callback = std::make_unique<typename ProxyBuilderBase<DummyReceiver>::BuilderCallback>(
            std::move(promise), std::move(user_callback));
        return on_found_builder_callback;
    }

    MockFunction<void(DummyReceiver&)> mock_user_callback{};
    bool was_destructor_called{false};
};

TEST_F(SingleImmediateInstantiationStrategyFixture, CallingFindWithShouldFindProxyTrueInvokesCallbackImmediately)
{
    // Given a SingleImmediateInstantiationStrategy which has kShouldFindProxy set to true
    constexpr bool kShouldFindProxy{true};
    SingleImmediateInstantiationStrategy<DummyReceiver, IsActiveReceiverMockFactory, kShouldFindProxy> strategy{};

    // Expecting that the user callback will be called
    EXPECT_CALL(mock_user_callback, Call(_));

    // When calling Find() with a BuilderCallback which wraps mocked user callback
    auto on_found_builder_callback = CreateBuilderCallbackWithMockedUserCallback();
    strategy.Find(std::move(on_found_builder_callback));
}

TEST_F(SingleImmediateInstantiationStrategyFixture, CallingFindWithShouldFindProxyFalseDoesNotInvokeCallback)
{
    // Given a SingleImmediateInstantiationStrategy which has kShouldFindProxy set to false
    constexpr bool kShouldFindProxy{false};
    SingleImmediateInstantiationStrategy<DummyReceiver, IsActiveReceiverMockFactory, kShouldFindProxy> strategy{};

    // Expecting that the user callback will not be called
    EXPECT_CALL(mock_user_callback, Call(_)).Times(0);

    // When calling Find() with a BuilderCallback which wraps mocked user callback
    auto on_found_builder_callback = CreateBuilderCallbackWithMockedUserCallback();
    strategy.Find(std::move(on_found_builder_callback));
}

TEST_F(SingleImmediateInstantiationStrategyFixture, CallingFindWithShouldFindProxyFalseStoresCallback)
{
    // Given a SingleImmediateInstantiationStrategy which has kShouldFindProxy set to false
    constexpr bool kShouldFindProxy{false};
    SingleImmediateInstantiationStrategy<DummyReceiver, IsActiveReceiverMockFactory, kShouldFindProxy> strategy{};

    // When calling Find() with a BuilderCallback which wraps a user callback containing a DestructorRecorder
    auto on_found_builder_callback = CreateBuilderCallbackWithDestructorRecordingUserCallback();
    strategy.Find(std::move(on_found_builder_callback));

    // Then the user callback should be stored within the strategy object (i.e. it should not be destroyed)
    EXPECT_FALSE(was_destructor_called);
}

TEST_F(SingleImmediateInstantiationStrategyFixture, CallingStopFindDestroysStoredCallback)
{
    // Given a SingleImmediateInstantiationStrategy which has kShouldFindProxy set to false
    constexpr bool kShouldFindProxy{false};
    SingleImmediateInstantiationStrategy<DummyReceiver, IsActiveReceiverMockFactory, kShouldFindProxy> strategy{};

    // and given Find() was previously called with a BuilderCallback which wraps a user callback containing a
    // DestructorRecorder
    auto on_found_builder_callback = CreateBuilderCallbackWithDestructorRecordingUserCallback();
    strategy.Find(std::move(on_found_builder_callback));

    // When calling StopFind
    EXPECT_FALSE(was_destructor_called);
    strategy.StopFind();

    // Then the user callback which was stored within the strategy object should be destroyed
    EXPECT_TRUE(was_destructor_called);
}

TEST_F(SingleImmediateInstantiationStrategyFixture, StoredCallbackIsDestroyedOnDestruction)
{
    // Given a SingleImmediateInstantiationStrategy which has kShouldFindProxy set to false
    constexpr bool kShouldFindProxy{false};
    auto strategy = std::make_unique<
        SingleImmediateInstantiationStrategy<DummyReceiver, IsActiveReceiverMockFactory, kShouldFindProxy>>();

    // and given Find() was previously called with a BuilderCallback which wraps a user callback containing a
    // DestructorRecorder
    auto on_found_builder_callback = CreateBuilderCallbackWithDestructorRecordingUserCallback();
    strategy->Find(std::move(on_found_builder_callback));

    // When destroying the strategy
    EXPECT_FALSE(was_destructor_called);
    strategy.reset();

    // Then the user callback which was stored within the strategy object should be destroyed
    EXPECT_TRUE(was_destructor_called);
}

TEST_F(SingleImmediateInstantiationStrategyFixture, ConstructingWithFactoryInstanceInvokesProvidedFactoryOnFind)
{
    // Given a SingleImmediateInstantiationStrategy constructed with a RecordingReceiverFactory instance
    bool was_factory_invoked{false};
    SingleImmediateInstantiationStrategy<DummyReceiver, RecordingReceiverFactory> strategy{
        RecordingReceiverFactory{was_factory_invoked}};

    // When calling Find() with a BuilderCallback which wraps mocked user callback
    auto on_found_builder_callback = CreateBuilderCallbackWithMockedUserCallback();
    strategy.Find(std::move(on_found_builder_callback));

    // Then the factory instance provided via the constructor should have been invoked
    EXPECT_TRUE(was_factory_invoked);
}

}  // namespace
}  // namespace score::mw::service::test
