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
///
/// @file
/// @copyright Copyright (C) 2023, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
///

#ifndef SCORE_MW_COM_IMPL_GENERIC_PROXY_EVENT_H
#define SCORE_MW_COM_IMPL_GENERIC_PROXY_EVENT_H

#include "score/mw/com/impl/generic_proxy_event_binding.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_event_base.h"
#include "score/mw/com/impl/tracing/proxy_event_tracing.h"
#include "score/mw/com/impl/tracing/proxy_event_tracing_data.h"

#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

/// \brief This is the user-visible class of an event that is part of a generic proxy. We make the distinction in the
/// ProxyEvent between functionality that is type aware and type agnostic. All type aware functionality is implemented
/// in ProxyEvent while all type agnostic functionality is implemented in the base class, ProxyEventBase. Since
/// GenericProxyEvent is the generic analogue of a ProxyEvent, it contains the same public interface as a ProxyEvent.
///
/// The class itself is a concrete type. However, it delegates all actions to an implementation
/// that is provided by the binding the proxy is operating on.
class GenericProxyEvent : public ProxyEventBase
{
  public:
    /// Constructor that allows to set the binding directly.
    ///
    /// This is used for testing only. Allows for directly setting the binding, and usually the mock binding is used
    /// here.
    ///
    /// \param proxy_binding The binding that shall be associated with this proxy.
    explicit GenericProxyEvent(ProxyBase& base,
                               std::unique_ptr<GenericProxyEventBinding> proxy_binding,
                               const std::string_view event_name);

    /// \brief Constructs a ProxyEvent by querying the base proxie's ProxyBinding for the respective ProxyEventBinding.
    ///
    /// \param parent Proxy that contains this event
    /// \param event_name Event name of the event, taken from the AUTOSAR model
    GenericProxyEvent(ProxyBase& base, const std::string_view event_name);

    /// \brief A ProxyEventBase shall not be copyable or copyable
    GenericProxyEvent(const GenericProxyEvent&) = delete;
    GenericProxyEvent& operator=(const GenericProxyEvent&) = delete;
    GenericProxyEvent(GenericProxyEvent&&) = delete;
    GenericProxyEvent& operator=(GenericProxyEvent&&) = delete;

    ~GenericProxyEvent() override = default;

    /// \brief Receive pending data from the event.
    ///
    /// The user needs to provide a callable that fulfills the following signature:
    /// void F(SamplePtr<void>) noexcept. This callback will be called for each sample
    /// that is available at the time of the call. Notice that the number of callback calls cannot
    /// exceed std::min(GetFreeSampleCount(), max_num_samples) times.
    ///
    /// \tparam F Callable with the signature void(SamplePtr<void>) noexcept
    /// \param receiver Callable with the appropriate signature. GetNewSamples will take ownership
    ///                 of this callable.
    /// \param max_num_samples Maximum number of samples to return via the given callable.
    /// \return Number of samples that were handed over to the callable or an error.
    template <typename F>
    Result<std::size_t> GetNewSamples(F&& receiver, std::size_t max_num_samples) noexcept;

    /// \brief return the (aligned) size in bytes of the underlying event sample data type.
    /// \return size in bytes.
    std::size_t GetSampleSize() const noexcept;

    /// \brief reports, whether the event sample data the SamplePtr<void> points to is in some internal serialized
    ///        format (true) or it is the binary representation of the underlying C++ data type (false).
    /// \return true in case the sample data is in some serialized format, false else.
    bool HasSerializedFormat() const noexcept;
};

template <typename F>
Result<std::size_t> GenericProxyEvent::GetNewSamples(F&& receiver, std::size_t max_num_samples) noexcept
{
    auto guard_factory{tracker_->Allocate(max_num_samples)};
    if (guard_factory.GetNumAvailableGuards() == 0U)
    {
        score::mw::log::LogWarn("lola")
            << "Unable to emit new samples, no free sample slots for this subscription available.";
        return MakeUnexpected(ComErrc::kMaxSamplesReached);
    }

    auto tracing_receiver =
        tracing::CreateTracingGenericGetNewSamplesCallback<F>(tracing_data_, std::forward<F>(receiver));

    auto* const proxy_event_binding = dynamic_cast<GenericProxyEventBinding*>(binding_base_.get());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(proxy_event_binding != nullptr, "Downcast to GenericProxyEventBinding failed!");
    const auto get_new_samples_result = proxy_event_binding->GetNewSamples(std::move(tracing_receiver), guard_factory);
    if (!get_new_samples_result.has_value())
    {
        if (get_new_samples_result.error() == ComErrc::kNotSubscribed)
        {
            return get_new_samples_result;
        }
        else
        {
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
    }
    return get_new_samples_result;
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_GENERIC_PROXY_EVENT_H
