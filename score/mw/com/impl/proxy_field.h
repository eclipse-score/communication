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
#ifndef SCORE_MW_COM_IMPL_PROXY_FIELD_H
#define SCORE_MW_COM_IMPL_PROXY_FIELD_H

#include "score/mw/com/impl/plumbing/proxy_field_binding_factory.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/proxy_event_binding.h"

#include "score/result/result.h"

#include <cstddef>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

// Suppress "AUTOSAR C++14 M3-2-3" rule finding. This rule states: "An identifier with external linkage shall have
// exactly one definition". This a forward class declaration.
// coverity[autosar_cpp14_m3_2_3_violation]
template <typename FieldType>
class ProxyFieldAttorney;

/// \brief This is the user-visible class of a field that is part of a proxy. It delegates all functionality to
/// ProxyEvent.
///
/// \tparam SampleDataType Type of data that is transferred by the event.
template <typename SampleDataType>
class ProxyField final
{

    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // Design dessision: The "*Attorney" class is a helper, which sets the internal state of this class accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyFieldAttorney<SampleDataType>;

  public:
    using FieldType = SampleDataType;

    /// Constructor that allows to set the binding directly.
    ///
    /// This is used for testing only. Allows for directly setting the binding, and usually the mock binding is used
    /// here.
    ///
    /// \param proxy_binding The binding that shall be associated with this proxy.
    ProxyField(ProxyBase& base,
               std::unique_ptr<ProxyEventBinding<FieldType>> proxy_binding,
               const std::string_view field_name)
        : proxy_event_dispatch_{base, std::move(proxy_binding), field_name}
    {
    }

    /// \brief Constructs a ProxyField
    ///
    /// \param base Proxy that contains this field
    /// \param field_name Field name of the field, taken from the AUTOSAR model
    ProxyField(ProxyBase& base, const std::string_view field_name)
        : proxy_event_dispatch_{base,
                                ProxyFieldBindingFactory<FieldType>::CreateEventBinding(base, field_name),
                                field_name,
                                typename ProxyEvent<FieldType>::PrivateConstructorEnabler{}}
    {
    }

    /// \brief A ProxyField shall not be copyable
    ProxyField(const ProxyField&) = delete;
    ProxyField& operator=(const ProxyField&) = delete;

    /// \brief A ProxyField shall be moveable
    ProxyField(ProxyField&&) noexcept = default;
    ProxyField& operator=(ProxyField&&) noexcept = default;

    ~ProxyField() noexcept = default;

    /// Subscribe to the field.
    ///
    /// \param max_sample_count Specify the maximum number of concurrent samples that this event shall
    ///                         be able to offer to the using application.
    /// \return On failure, returns an error code.
    ResultBlank Subscribe(const std::size_t max_sample_count) noexcept
    {
        return proxy_event_dispatch_.Subscribe(max_sample_count);
    }

    /// \brief Get the subscription state of this field.
    ///
    /// This method can always be called regardless of the state of the field.
    ///
    /// \return Subscription state of the field.
    SubscriptionState GetSubscriptionState() const noexcept
    {
        return proxy_event_dispatch_.GetSubscriptionState();
    }

    /// \brief End subscription to a field and release needed resources.
    ///
    /// It is illegal to call this method while data is still held by the application in the form of SamplePtr. Doing so
    /// will result in undefined behavior.
    ///
    /// After a call to this method, the field behaves as if it had just been constructed.
    void Unsubscribe() noexcept
    {
        proxy_event_dispatch_.Unsubscribe();
    }

    /// \brief Get the number of samples that can still be received by the user of this field.
    ///
    /// If this returns 0, the user first has to drop at least one SamplePtr before it is possible to receive data via
    /// GetNewSamples again. If there is no subscription for this field, the returned value is unspecified.
    ///
    /// \return Number of samples that can still be received.
    std::size_t GetFreeSampleCount() const noexcept
    {
        return proxy_event_dispatch_.GetFreeSampleCount();
    }

    /// \brief Returns the number of new samples a call to GetNewSamples() (given parameter max_num_samples
    /// doesn't restrict it) would currently provide.
    ///
    /// \details This is a proprietary extension to the official ara::com API. It is useful in resource sensitive
    ///          setups, where the user wants to work in polling mode only without registered async receive-handlers.
    ///          For further details see //score/mw/com/design/extensions/README.md.
    ///
    /// \return Either 0 if no new samples are available (and GetNewSamples() wouldn't return any) or N, where 1 <= N <=
    /// actual new samples. I.e. an implementation is allowed to report a lower number than actual new samples, which
    /// would be provided by a call to GetNewSamples().
    Result<std::size_t> GetNumNewSamplesAvailable() const noexcept
    {
        return proxy_event_dispatch_.GetNumNewSamplesAvailable();
    }

    /// \brief Receive pending data from the field.
    ///
    /// The user needs to provide a callable that fulfills the following signature:
    /// void F(SamplePtr<const FieldType>) noexcept. This callback will be called for each sample
    /// that is available at the time of the call. Notice that the number of callback calls cannot
    /// exceed std::min(GetFreeSampleCount(), max_num_samples) times.
    ///
    /// \tparam F Callable with the signature void(SamplePtr<const FieldType>) noexcept
    /// \param receiver Callable with the appropriate signature. GetNewSamples will take ownership
    ///                 of this callable.
    /// \param max_num_samples Maximum number of samples to return via the given callable.
    /// \return Number of samples that were handed over to the callable or an error.
    template <typename F>
    Result<std::size_t> GetNewSamples(F&& receiver, const std::size_t max_num_samples) noexcept
    {
        return proxy_event_dispatch_.GetNewSamples(std::forward<F>(receiver), max_num_samples);
    }

    ResultBlank SetReceiveHandler(EventReceiveHandler handler) noexcept
    {
        return proxy_event_dispatch_.SetReceiveHandler(std::move(handler));
    }

    ResultBlank UnsetReceiveHandler() noexcept
    {
        return proxy_event_dispatch_.UnsetReceiveHandler();
    }

  private:
    ProxyEvent<FieldType> proxy_event_dispatch_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXY_FIELD_H
