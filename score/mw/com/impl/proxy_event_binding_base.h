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
#ifndef SCORE_MW_COM_IMPL_PROXY_EVENT_BINDING_BASE_H
#define SCORE_MW_COM_IMPL_PROXY_EVENT_BINDING_BASE_H

#include "score/mw/com/impl/binding_type.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"
#include "score/mw/com/impl/subscription_state.h"

#include "score/result/result.h"

#include <cstddef>
#include <memory>

namespace score::mw::com::impl
{

/// \brief This is the binding independent, type-agnostic base class for all proxy events inside a proxy.
///
/// This class contains all type-agnostic, public, user-facing methods of proxy events and all method
/// signatures that need to be implemented by the bindings. This class is missing GetNewSamples because this method (as
/// well as the callback involved) needs type information.
class ProxyEventBindingBase
{
  public:
    // A ProxyEventBindingBase is always held via a pointer in the binding independent impl::ProxyEventBase.
    // Therefore, the binding itself doesn't have to be moveable or copyable, as the pointer can simply be copied when
    // moving the impl::ProxyEventBase.
    ProxyEventBindingBase(const ProxyEventBindingBase&) = delete;
    ProxyEventBindingBase(ProxyEventBindingBase&&) noexcept = delete;
    ProxyEventBindingBase& operator=(ProxyEventBindingBase&&) & noexcept = delete;
    ProxyEventBindingBase& operator=(const ProxyEventBindingBase&) & = delete;

    virtual ~ProxyEventBindingBase() noexcept;

    /// Subscribe to the event.
    ///
    /// This will initialize the event so that event data can be received once it arrives.
    ///
    /// \param max_sample_count Specify the maximum number of concurrent samples that this event shall
    ///                         be able to offer to the using application.
    virtual ResultBlank Subscribe(const std::size_t max_sample_count) noexcept = 0;

    /// \brief Get the subscription state of this event.
    ///
    /// This method can always be called regardless of the state of the event.
    ///
    /// \return Subscription status.
    virtual SubscriptionState GetSubscriptionState() const noexcept = 0;

    /// \brief End subscription to an event and release needed resources.
    ///
    /// After a call to this method, the event behaves as if it had just been constructed.
    virtual void Unsubscribe() noexcept = 0;

    /// Set a callback that is called whenever at least one new sample can be retrieved from the event.
    ///
    /// The handler must not throw an exception and it must not terminate.
    ///
    /// \param handler The callback to be called on event reception.
    virtual ResultBlank SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept = 0;

    /// \brief Remove any receive handler registered via SetReceiveHandler()
    virtual ResultBlank UnsetReceiveHandler() noexcept = 0;

    /// \brief Returns the number of new samples a call to GetNewSamples() would currently provide if the
    /// max_sample_count set in the Subscribe call and GetNewSamples call were both infinitely high.
    /// \see ProxyEvent::GetNumNewSamplesAvailable()
    ///
    /// \return Either 0 if no new samples are available (and GetNewSamples() wouldn't return any) or N, where 1 <= N <=
    /// actual new samples. I.e. an implementation is allowed to report a lower number than actual new samples, which
    /// would be provided by a call to GetNewSamples().
    virtual Result<std::size_t> GetNumNewSamplesAvailable() const noexcept = 0;

    /// \brief Returns the current max sample count that was provided in the Subscribe call that was most recently
    /// processed or is currently processing.
    ///
    /// \return If GetSubscriptionState() is currently kSubscribed or kSubscriptionPending, returns the max_sample_count
    /// that was passed to the Subscribe call. Otherwise, returns empty.
    virtual std::optional<std::uint16_t> GetMaxSampleCount() const noexcept = 0;

    /// \brief Gets the binding type of the binding
    virtual BindingType GetBindingType() const noexcept = 0;

    /// \brief Notifies the event that the provider service instance that it is connected to (i.e. the
    ///        SkeletonEvent) has changed its availability.
    /// \param is_available true if the provider service instance has changed from being unavailable to available. false
    ///        if the providing service instance has changed from being available to unavailable.
    /// \param new_event_source_pid new pid of provider service instance.
    ///
    /// This will be called by the Proxy which will begin a StartFindService search on construction for the provider
    /// service instance. When the service instance changes availability, it will trigger a callback that will call
    /// NotifyServiceInstanceChangedAvailability for all service elements contained within the Proxy.
    virtual void NotifyServiceInstanceChangedAvailability(bool is_available, pid_t new_event_source_pid) noexcept = 0;

  protected:
    ProxyEventBindingBase() = default;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXY_EVENT_BINDING_BASE_H
