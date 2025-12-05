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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_PROXY_EVENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_PROXY_EVENT_H

#include "score/mw/com/impl/bindings/mock_binding/sample_ptr.h"
#include "score/mw/com/impl/proxy_event_binding.h"
#include "score/mw/com/impl/proxy_event_binding_base.h"

#include <score/assert.hpp>

#include <gmock/gmock.h>
#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

namespace score::mw::com::impl::mock_binding
{

class ProxyEventBase : public ProxyEventBindingBase
{
  public:
    ~ProxyEventBase() override = default;

    MOCK_METHOD(SubscriptionState, GetSubscriptionState, (), (const, noexcept, override));
    MOCK_METHOD(void, Unsubscribe, (), (noexcept, override));
    MOCK_METHOD(ResultBlank, Subscribe, (std::size_t), (noexcept, override));
    MOCK_METHOD(Result<std::size_t>, GetNumNewSamplesAvailable, (), (const, noexcept, override));
    MOCK_METHOD(ResultBlank, SetReceiveHandler, (std::weak_ptr<ScopedEventReceiveHandler>), (noexcept, override));
    MOCK_METHOD(ResultBlank, UnsetReceiveHandler, (), (noexcept, override));
    MOCK_METHOD(std::optional<std::uint16_t>, GetMaxSampleCount, (), (const, noexcept, override));
    MOCK_METHOD(BindingType, GetBindingType, (), (const, noexcept, override));
    MOCK_METHOD(void, NotifyServiceInstanceChangedAvailability, (bool, pid_t), (noexcept, override));
};

/// \brief Mock implementation for proxy event bindings.
///
/// This mock also includes a default behavior for GetNewSamples(): If there are fake samples added to an internal FIFO,
/// these samples are returned in order to the provided callback, unless stated otherwise with EXPECT_CALL.
///
/// \tparam SampleType Data type to be received by this proxy event.
template <typename SampleType>
class ProxyEvent : public ProxyEventBinding<SampleType>
{
  public:
    ProxyEvent() : ProxyEventBinding<SampleType>{}
    {
        ON_CALL(*this, GetNewSamples(::testing::_, ::testing::_))
            .WillByDefault(
                ::testing::WithArgs<0, 1>(::testing::Invoke(this, &ProxyEvent<SampleType>::GetNewFakeSamples)));
    }

    ~ProxyEvent() = default;

    MOCK_METHOD(SubscriptionState, GetSubscriptionState, (), (const, noexcept, override));
    MOCK_METHOD(void, Unsubscribe, (), (noexcept, override));
    MOCK_METHOD(ResultBlank, Subscribe, (std::size_t), (noexcept, override));
    MOCK_METHOD(Result<std::size_t>, GetNumNewSamplesAvailable, (), (const, noexcept, override));
    MOCK_METHOD(Result<std::size_t>,
                GetNewSamples,
                (typename ProxyEventBinding<SampleType>::Callback&&, TrackerGuardFactory&),
                (noexcept, override));
    MOCK_METHOD(ResultBlank, SetReceiveHandler, (std::weak_ptr<ScopedEventReceiveHandler>), (noexcept, override));
    MOCK_METHOD(ResultBlank, UnsetReceiveHandler, (), (noexcept, override));
    MOCK_METHOD(std::optional<std::uint16_t>, GetMaxSampleCount, (), (const, noexcept, override));
    MOCK_METHOD(BindingType, GetBindingType, (), (const, noexcept, override));
    MOCK_METHOD(void, NotifyServiceInstanceChangedAvailability, (bool, pid_t), (noexcept, override));

    /// \brief Add a sample to the internal queue of fake events.
    ///
    /// On a call to GetNewSamples(), these samples will be forwarded to the provided callable in case there is no
    /// EXPECT_CALL that overrides this behavior. This can be used to simulate received data on the proxy side.
    ///
    /// \param sample The data to be added to the incoming queue.
    void PushFakeSample(SampleType&& sample)
    {
        fake_samples_.push_back(std::make_unique<SampleType>(std::forward<SampleType>(sample)));
    }

  private:
    using FakeSamples = std::vector<SamplePtr<SampleType>>;
    FakeSamples fake_samples_{};

    Result<std::size_t> GetNewFakeSamples(typename ProxyEventBinding<SampleType>::Callback&& callable,
                                          TrackerGuardFactory& tracker);
};

template <typename SampleType>
Result<std::size_t> ProxyEvent<SampleType>::GetNewFakeSamples(
    typename ProxyEventBinding<SampleType>::Callback&& callable,
    TrackerGuardFactory& tracker)
{
    const std::size_t num_samples = std::min(fake_samples_.size(), tracker.GetNumAvailableGuards());
    const auto start_iter = fake_samples_.end() - static_cast<typename FakeSamples::difference_type>(num_samples);
    auto sample = std::make_move_iterator(start_iter);

    while (sample.base() != fake_samples_.end())
    {
        std::optional<SampleReferenceGuard> guard = tracker.TakeGuard();
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(guard.has_value(), "No guards available.");
        SamplePtr<SampleType> ptr = *sample;
        impl::SamplePtr<SampleType> impl_ptr = this->MakeSamplePtr(std::move(ptr), std::move(*guard));

        const tracing::ITracingRuntime::TracePointDataId dummy_trace_point_data_id{0U};
        callable(std::move(impl_ptr), dummy_trace_point_data_id);
        ++sample;
    }

    fake_samples_.clear();
    return {num_samples};
}

template <typename SampleType>
class ProxyEventFacade : public ProxyEventBinding<SampleType>
{
  public:
    ProxyEventFacade(ProxyEvent<SampleType>& proxy_event)
        : ProxyEventBinding<SampleType>{}, proxy_event_{proxy_event} {};

    ~ProxyEventFacade() override = default;

    SubscriptionState GetSubscriptionState() const noexcept override
    {
        return proxy_event_.GetSubscriptionState();
    }
    void Unsubscribe() noexcept override
    {
        return proxy_event_.Unsubscribe();
    }
    ResultBlank Subscribe(std::size_t n) noexcept override
    {
        return proxy_event_.Subscribe(n);
    }
    Result<std::size_t> GetNumNewSamplesAvailable() const noexcept override
    {
        return proxy_event_.GetNumNewSamplesAvailable();
    }
    Result<std::size_t> GetNewSamples(typename ProxyEventBinding<SampleType>::Callback&& callback,
                                      TrackerGuardFactory& tracker_guard_factory) noexcept override
    {
        return proxy_event_.GetNewSamples(std::move(callback), tracker_guard_factory);
    }
    ResultBlank SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept override
    {
        return proxy_event_.SetReceiveHandler(handler);
    }
    ResultBlank UnsetReceiveHandler() noexcept override
    {
        return proxy_event_.UnsetReceiveHandler();
    }
    std::optional<std::uint16_t> GetMaxSampleCount() const noexcept override
    {
        return proxy_event_.GetMaxSampleCount();
    }
    BindingType GetBindingType() const noexcept override
    {
        return proxy_event_.GetBindingType();
    }
    void NotifyServiceInstanceChangedAvailability(bool b, pid_t pid) noexcept override
    {
        return proxy_event_.NotifyServiceInstanceChangedAvailability(b, pid);
    }

  private:
    ProxyEvent<SampleType>& proxy_event_;
};

}  // namespace score::mw::com::impl::mock_binding

#endif  // SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_PROXY_EVENT_H
