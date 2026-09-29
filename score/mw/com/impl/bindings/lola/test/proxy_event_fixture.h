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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_PROXY_EVENT_FIXTURE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_PROXY_EVENT_FIXTURE_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/proxy.h"
#include "score/mw/com/impl/bindings/lola/proxy_event.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"
#include "score/mw/com/impl/sample_reference_tracker.h"
#include "score/mw/com/impl/subscription_state.h"

#include "score/language/safecpp/scoped_function/scope.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace score::mw::com::impl::lola
{

/// \brief Test attorney granting access to a Proxy's private map of registered ProxyEvents, so that tests can verify
/// that a ProxyEvent registered itself with its parent Proxy.
class ProxyTestAttorney
{
  public:
    explicit ProxyTestAttorney(Proxy& proxy) : proxy_{proxy} {}

    const std::unordered_map<std::string_view, std::reference_wrapper<ProxyEvent>>& GetEvents() const
    {
        return proxy_.proxy_events_;
    }

  private:
    Proxy& proxy_;
};

using TestSampleType = std::uint32_t;

constexpr std::size_t kMaxSampleCount{2U};

constexpr EventSlotStatus::EventTimeStamp kDummyInputTimestamp{10U};
constexpr TestSampleType kDummySampleValue{42U};

class CallbackCountingReceiver
{
  public:
    CallbackCountingReceiver(std::uint16_t& num_callbacks_called) : num_callbacks_called_{num_callbacks_called} {}

    void operator()(impl::SamplePtr<void>, const tracing::ITracingRuntime::TracePointDataId)
    {
        num_callbacks_called_++;
    }

  private:
    std::reference_wrapper<std::uint16_t> num_callbacks_called_;
};

/// \brief Function that casts and returns the value pointed to by a void pointer
///
/// Assumes that the object in memory being pointed to is of type TestSampleType.
inline TestSampleType GetSamplePtrValue(const void* const void_ptr)
{
    auto* const typed_ptr = static_cast<const TestSampleType*>(void_ptr);
    return *typed_ptr;
}

template <typename R, typename... Args>
std::shared_ptr<ScopedEventReceiveHandler> FromMockFunction(safecpp::Scope<>& event_receive_handler_scope,
                                                            ::testing::MockFunction<R(Args...)>& mock_function)
{
    return std::make_shared<ScopedEventReceiveHandler>(event_receive_handler_scope, [&mock_function](Args&&... args) {
        mock_function.Call(std::forward<Args>(args)...);
    });
}

/// \brief Test fixture for ProxyEvent functionality.
class LolaProxyEventFixture : public LolaProxyEventResources
{
  public:
    LolaProxyEventFixture& GivenAProxyEvent(const ElementFqId element_fq_id, const std::string& event_name)
    {
        test_proxy_event_ = std::make_unique<ProxyEvent>(*proxy_, element_fq_id, event_name);
        return *this;
    }

    LolaProxyEventFixture& ThatIsSubscribedWithMaxSamples(const std::size_t max_sample_count)
    {
        std::ignore = this->test_proxy_event_->Subscribe(max_sample_count);
        sample_reference_tracker_ = std::make_unique<SampleReferenceTracker>(max_sample_count);
        return *this;
    }

    LolaProxyEventFixture& ThatIsInSubscriptionPendingWithMaxSamples(const std::size_t max_sample_count)
    {
        ThatIsSubscribedWithMaxSamples(max_sample_count);
        const bool is_available = false;
        this->test_proxy_event_->NotifyServiceInstanceChangedAvailability(is_available,
                                                                          ProxyMockedMemoryFixture::kDummyPid);
        const auto new_subscription_state = this->test_proxy_event_->GetSubscriptionState();
        EXPECT_EQ(new_subscription_state, SubscriptionState::kSubscriptionPending);
        return *this;
    }

    LolaProxyEventFixture& WithSkeletonEventData(
        const std::vector<std::pair<TestSampleType, EventSlotStatus::EventTimeStamp>> data)
    {
        for (const auto& datum : data)
        {
            score::cpp::ignore = this->PutData(datum.first, datum.second);
        }
        return *this;
    }

    Result<std::size_t> GetNewSamples(
        std::function<void(impl::SamplePtr<void>, const tracing::ITracingRuntime::TracePointDataId)> receiver,
        const std::size_t max_num_samples)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT(test_proxy_event_ != nullptr);
        SCORE_LANGUAGE_FUTURECPP_ASSERT(sample_reference_tracker_ != nullptr);
        TrackerGuardFactory guard_factory{this->sample_reference_tracker_->Allocate(max_num_samples)};
        return test_proxy_event_->GetNewSamples(std::move(receiver), guard_factory);
    }

    // ProxyEvent no longer Unsubscribes on destruction, so do it explicitly.
    void TearDown() override
    {
        if (test_proxy_event_ != nullptr)
        {
            test_proxy_event_->Unsubscribe();
        }
        ProxyMockedMemoryFixture::TearDown();
    }

    std::unique_ptr<ProxyEvent> test_proxy_event_{nullptr};
    std::unique_ptr<SampleReferenceTracker> sample_reference_tracker_{};
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_TEST_PROXY_EVENT_FIXTURE_H
