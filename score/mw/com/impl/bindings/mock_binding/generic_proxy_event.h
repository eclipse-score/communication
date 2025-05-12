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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_GENERIC_PROXY_EVENT_H
#define SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_GENERIC_PROXY_EVENT_H

#include "score/mw/com/impl/bindings/mock_binding/sample_ptr.h"

#include "score/mw/com/impl/generic_proxy_event_binding.h"

#include "gmock/gmock.h"

#include <score/assert.hpp>

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

namespace score::mw::com::impl::mock_binding
{
/// \brief Mock implementation for generic proxy event bindings.
///
/// This mock also includes a default behavior for GetNewSamples(): If there are fake samples added to an internal FIFO,
/// these samples are returned in order to the provided callback, unless stated otherwise with EXPECT_CALL.
class GenericProxyEvent : public GenericProxyEventBinding
{
  public:
    GenericProxyEvent() : GenericProxyEventBinding{}, fake_samples_{}
    {
        ON_CALL(*this, GetNewSamples(::testing::_, ::testing::_))
            .WillByDefault(::testing::WithArgs<0, 1>(::testing::Invoke(this, &GenericProxyEvent::GetNewFakeSamples)));
    }

    ~GenericProxyEvent() = default;

    MOCK_METHOD(SubscriptionState, GetSubscriptionState, (), (const, noexcept, override));
    MOCK_METHOD(void, Unsubscribe, (), (noexcept, override));
    MOCK_METHOD(ResultBlank, Subscribe, (std::size_t), (noexcept, override));
    MOCK_METHOD(Result<std::size_t>, GetNumNewSamplesAvailable, (), (const, noexcept, override));
    MOCK_METHOD(std::size_t, GetSampleSize, (), (const, noexcept, override));
    MOCK_METHOD(bool, HasSerializedFormat, (), (const, noexcept, override));
    MOCK_METHOD(Result<std::size_t>,
                GetNewSamples,
                (typename GenericProxyEventBinding::Callback&&, TrackerGuardFactory&),
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
    /// \tparam SampleType Data type to be received by this proxy event.
    /// \param sample The data to be added to the incoming queue.
    template <typename SampleType>
    void PushFakeSample(SampleType&& sample)
    {
        SamplePtr<void> sample_ptr(new SampleType(std::forward<SampleType>(sample)), [](void* p) noexcept {
            auto* const int_p = static_cast<SampleType*>(p);
            delete int_p;
        });
        fake_samples_.push_back(std::move(sample_ptr));
    }

  private:
    using FakeSamples = std::vector<SamplePtr<void>>;
    FakeSamples fake_samples_;

    Result<std::size_t> GetNewFakeSamples(typename GenericProxyEventBinding::Callback&& callable,
                                          TrackerGuardFactory& tracker);
};

}  // namespace score::mw::com::impl::mock_binding

#endif  // SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_GENERIC_PROXY_EVENT_H
