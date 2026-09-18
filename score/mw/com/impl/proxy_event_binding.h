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
#ifndef SCORE_MW_COM_IMPL_PROXY_EVENT_BINDING_H
#define SCORE_MW_COM_IMPL_PROXY_EVENT_BINDING_H

#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/sample_reference_tracker.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"
#include "score/mw/com/impl/subscription_state.h"
#include "score/mw/com/impl/subscription_state_change_handler.h"
#include "score/mw/com/impl/tracing/i_tracing_runtime.h"

#include <score/callback.hpp>

#include <cstddef>
#include <utility>

namespace score::mw::com::impl
{

/// \brief Interface a proxy event binding implementation has to implement.
///
/// \details On binding level (Proxy)Events are type-erased. I.e. on binding level only bytes are stored/transported.
/// The APIs this interface provides, deal with void-pointers (SamplePtr), since the binding layer is type-erased! The
/// type knowledge for the events is held on the binding independent impl::ProxyEvent, which does the transformation/
/// rebind from type-erased (void) to typed (non-void) representation.
class ProxyEventBinding
{
  public:
    /// Type-erased callback used for the GetNewSamples method.
    ///
    /// The size of 80U is chosen to allow us to store another score::cpp::callback within this callback. This is needed
    /// as we wrap the user provided callback in order to perform tracing functionality.
    using Callback = score::cpp::callback<void(SamplePtr<void>, tracing::ITracingRuntime::TracePointDataId), 80U>;

    ProxyEventBinding() = default;
    // A ProxyEventBinding is always held via a pointer in the binding independent impl::ProxyEvent.
    // Therefore, the binding itself doesn't have to be moveable or copyable, as the pointer can simply be copied when
    // moving the impl::ProxyEvent.
    ProxyEventBinding(const ProxyEventBinding&) = delete;
    ProxyEventBinding(ProxyEventBinding&&) noexcept = delete;
    ProxyEventBinding& operator=(const ProxyEventBinding&) & = delete;
    ProxyEventBinding& operator=(ProxyEventBinding&&) & noexcept = delete;

    virtual ~ProxyEventBinding() noexcept;

    /// \brief Subscribe to the event.
    ///
    /// This will initialize the event so that event data can be received once it arrives.
    ///
    /// \param max_sample_count Specify the maximum number of concurrent samples that this event shall
    ///                         be able to offer to the using application.
    virtual Result<void> Subscribe(const std::size_t max_sample_count) noexcept = 0;

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
    virtual Result<void> SetReceiveHandler(std::weak_ptr<ScopedEventReceiveHandler> handler) noexcept = 0;

    /// \brief Remove any receive handler registered via SetReceiveHandler()
    virtual Result<void> UnsetReceiveHandler() noexcept = 0;

    /// \brief Sets/Registers a SubscriptionStateChangeHandler for this event. This handler will be called whenever the
    /// subscription state of this event changes.
    /// \note An already set/registered SubscriptionStateChangeHandler will be silently overridden.
    /// @param handler The callback to be called in case of a subscription state change.
    virtual Result<void> SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler handler) noexcept = 0;

    /// \brief Remove any receive handler registered via SetSubscriptionStateChangeHandler()
    virtual Result<void> UnsetSubscriptionStateChangeHandler() noexcept = 0;

    /// \brief Returns the number of new samples a call to GetNewSamples() would currently provide if the
    /// max_sample_count set in the Subscribe call and GetNewSamples call were both infinitely high.
    /// \see ProxyEvent::GetNumNewSamplesAvailable()
    ///
    /// \return Either 0 if no new samples are available (and GetNewSamples() wouldn't return any) or N, where 1 <= N <=
    /// actual new samples. I.e. an implementation is allowed to report a lower number than actual new samples, which
    /// would be provided by a call to GetNewSamples().
    virtual Result<std::size_t> GetNumNewSamplesAvailable() const = 0;

    /// \brief Returns the current max sample count that was provided in the Subscribe call that was most recently
    /// processed or is currently processing.
    ///
    /// \return If GetSubscriptionState() is currently kSubscribed or kSubscriptionPending, returns the max_sample_count
    /// that was passed to the Subscribe call. Otherwise, returns empty.
    virtual std::optional<std::uint16_t> GetMaxSampleCount() const noexcept = 0;

    /// \brief Gets the binding type of the binding
    virtual BindingType GetBindingType() const noexcept = 0;

    /// \brief Get pending data from the event.
    ///
    /// The user needs to provide a callback which will be called for each sample
    /// that is available at the time of the call. Notice that the number of callback calls cannot
    /// exceed std::min(GetFreeSampleCount(), max_num_samples) times.
    ///
    /// \param receiver Callback that will be used to hand over data to the upper layer.
    /// \param tracker Tracker that is used to produce reference counted SamplePtrs.
    /// \return Number of samples that were handed over to the callable.
    virtual Result<std::size_t> GetNewSamples(Callback&& receiver, TrackerGuardFactory& tracker) noexcept = 0;

  protected:
    /// Create a binding-independent SamplePtr from a binding-specific sample pointer.
    ///
    /// This serves as a placeholder to facilitate more complex construction in the future (read: when reference
    /// counting will be implemented for the proxy side).
    ///
    /// \tparam BindingSamplePtr The sample pointer from the binding.
    /// \param binding_ptr Type of the binding-specific sample pointer.
    /// \param reference_guard Reference counting guard managing the count of SamplePtrs that are alive.
    /// \return Binding-independent SamplePtr instance.
    template <typename BindingSamplePtr>
    static SamplePtr<void> MakeSamplePtr(BindingSamplePtr&& binding_ptr, SampleReferenceGuard reference_guard) noexcept;
};

template <typename BindingSamplePtr>
inline SamplePtr<void> ProxyEventBinding::MakeSamplePtr(BindingSamplePtr&& binding_ptr,
                                                        SampleReferenceGuard reference_guard) noexcept
{
    return SamplePtr<void>{std::forward<BindingSamplePtr>(binding_ptr), std::move(reference_guard)};
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXY_EVENT_BINDING_H
