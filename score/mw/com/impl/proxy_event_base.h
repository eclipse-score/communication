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

#ifndef SCORE_MW_COM_IMPL_PROXY_EVENT_BASE_H
#define SCORE_MW_COM_IMPL_PROXY_EVENT_BASE_H

#include "score/mw/com/impl/event_receive_handler.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_event_binding_base.h"
#include "score/mw/com/impl/sample_reference_tracker.h"
#include "score/mw/com/impl/subscription_state.h"
#include "score/mw/com/impl/tracing/proxy_event_tracing_data.h"

#include "score/language/safecpp/scoped_function/scope.h"
#include "score/result/result.h"

#include <cstddef>
#include <memory>

namespace score::mw::com::impl
{

class EventBindingRegistrationGuard;

/// \brief This is the user-visible class of an event that is part of a proxy. It contains ProxyEvent functionality that
/// is agnostic of the data type that is transferred by the event.
///
/// The class itself is a concrete type. However, it delegates all actions to an implementation
/// that is provided by the binding the proxy is operating on.
class ProxyEventBase
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // Design dessision: The "*Attorney" class is a helper, which sets the internal state of this class accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyEventBaseAttorney;

  public:
    ProxyEventBase(ProxyBase& proxy_base,
                   std::unique_ptr<ProxyEventBindingBase> proxy_event_binding,
                   score::cpp::string_view event_name) noexcept;

    /// \brief A ProxyEventBase shall not be copyable
    ProxyEventBase(const ProxyEventBase&) = delete;
    ProxyEventBase& operator=(const ProxyEventBase&) = delete;

    /// \brief A ProxyEventBase shall be moveable
    // Suppress "AUTOSAR C++14 M3-2-2": "The One Definition Rule shall not be violated.";
    // "AUTOSAR C++14 M3-2-4": "An identifier with external linkage shall have exactly one definition".
    // All these findings are false-positives.
    // Suppress "AUTOSAR C++14 A12-8-6": "Copy and move constructors and copy assignment and move assignment operators
    // shall be declared protected or defined “=delete” in base class". Movable constructors cannot be protected, it
    // cannot be tested if not public.
    // coverity[autosar_cpp14_a12_8_6_violation]
    // coverity[autosar_cpp14_m3_2_2_violation]
    // coverity[autosar_cpp14_m3_2_4_violation]
    ProxyEventBase(ProxyEventBase&&) noexcept;
    // coverity[autosar_cpp14_a12_8_6_violation]
    // coverity[autosar_cpp14_m3_2_2_violation]
    // coverity[autosar_cpp14_m3_2_4_violation]
    ProxyEventBase& operator=(ProxyEventBase&&) noexcept;

    virtual ~ProxyEventBase() noexcept;

    /// Subscribe to the event.
    ///
    /// This will initialize the event so that event data can be received once it arrives.
    ///
    /// \param max_sample_count Specify the maximum number of concurrent samples that this event shall
    ///                         be able to offer to the using application.
    /// \return On failure, returns an error code.
    ResultBlank Subscribe(const std::size_t max_sample_count) noexcept;

    /// \brief Get the subscription state of this event.
    ///
    /// This method can always be called regardless of the state of the event.
    ///
    /// \return Subscription state of the event.
    SubscriptionState GetSubscriptionState() const noexcept
    {
        return binding_base_->GetSubscriptionState();
    }

    /// \brief End subscription to an event and release needed resources.
    ///
    /// It is illegal to call this method while data is still held by the application in the form of SamplePtr. Doing so
    /// will result in undefined behavior.
    /// An eventually currently registered ReceiveHandler will get removed (needs to be set again for a new
    /// subscription) and therefore, this method will "synchronize" with a currently running ReceiveHandler
    /// and will only finish after any running ReceiveHandler has ended.
    ///
    /// After a call to this method, the event behaves as if it had just been constructed.
    void Unsubscribe() noexcept;

    /// \brief Get the number of samples that can still be received by the user of this event.
    ///
    /// If this returns 0, the user first has to drop at least one SamplePtr before it is possible to receive data via
    /// GetNewSamples again. If there is no subscription for this event, the returned value is unspecified.
    ///
    /// \return Number of samples that can still be received.
    std::size_t GetFreeSampleCount() const noexcept
    {
        return tracker_->GetNumAvailableSamples();
    }

    /// \brief Returns the number of new samples a call to GetNewSamples() would currently provide if the
    /// max_sample_count set in the Subscribe call and GetNewSamples call were both infinitely high.
    ///
    /// \details E.g. If there are 10 available / valid samples, but the max_sample_count set in the Subscribe() call
    /// was 2, then GetNumNewSamplesAvailable() would return 10 while a call to GetNewSamples(2) would only receive 2
    /// samples.
    //  This is a proprietary extension to the official ara::com API. It is useful in resource sensitive
    /// setups, where the user wants to work in polling mode only without registered async receive-handlers.
    /// For further details see //score/mw/com/design/extensions/README.md.
    ///
    /// \return Either 0 if no new samples are available (and GetNewSamples() wouldn't return any) or N, where 1 <= N <=
    /// actual new samples. I.e. an implementation is allowed to report a lower number than actual new samples, which
    /// would be provided by a call to GetNewSamples().
    Result<std::size_t> GetNumNewSamplesAvailable() const noexcept;

    /// \brief Sets the handler to be called, whenever a new event-sample has been received.
    /// \details Generally a ReceiveHandler has no restrictions on what mw::com API it is allowed to call.
    ///          It is especially allowed to call all public APIs of the Event instance on which it had been
    ///          set/registered as long as it obeys the general requirement, that API calls on a Proxy/Proxy event are
    ///          thread safe/can't be called concurrently.
    /// \attention This function shall MUST NOT be called from the context of a ReceiveHandler registered for this
    ///            event!
    ///            It makes semantically not really sense to register a "new" ReceiveHandler from the context of an
    ///            already running ReceiveHandler. We also see no use cases for it and won't support it therefore.
    /// \param handler user provided handler to be called
    ResultBlank SetReceiveHandler(EventReceiveHandler handler) noexcept;

    /// \brief Removes any ReceiveHandler registered via #SetReceiveHandler
    ResultBlank UnsetReceiveHandler() noexcept;

    bool IsBindingValid() const noexcept
    {
        return binding_base_ != nullptr;
    }

  protected:
    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". We need these data elements to exchange this information between the ProxyEventBase and the
    // GenericProxyEvent.
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::unique_ptr<ProxyEventBindingBase> binding_base_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::unique_ptr<SampleReferenceTracker> tracker_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    tracing::ProxyEventTracingData tracing_data_;
    // coverity[autosar_cpp14_m11_0_1_violation]
    std::unique_ptr<EventBindingRegistrationGuard> event_binding_registration_guard_;

  private:
    /// \brief Expires the #receive_handler_scope_ in case not being called in the context of an EventReceiveHandler
    ///        (because trying to expire the scope in which we are running, would lead to a deadlock)
    void ExpireReceiveHandlerScopeIfNotInHandler();

    safecpp::Scope<> receive_handler_scope_;
    std::shared_ptr<ScopedEventReceiveHandler> receive_handler_ptr_;
    /// \brief thread local variable, which indicates, whether the current thread is within the call context of an
    ///        user provided EventReceiveHandler registered via #SetReceiveHandler.
    /// \details In #UnsetReceiveHandler (or #Unsubscribe, which also implicitly does an UnsetReceiveHandler), we need
    ///          to synchronize with an (maybe) currently running user provided EventReceiveHandler, which is
    ///          encapsulated within a MoveOnlyScopedFunction. This is realized by expiring the scope
    ///          (#receive_handler_scope_), which this MoveOnlyScopedFunction is bound to.
    ///          But we can't do this expiry, when UnsetReceiveHandler/Unsubscribe is called within the context of the
    ///          MoveOnlyScopedFunction itself! Because this would "deadlock" -> scope.expire() is a blocking method,
    ///          which waits for the MoveOnlyScopedFunction to finish ...
    ///          But since we allow the mw::com user to call UnsetReceiveHandler/Unsubscribe in his provided
    ///          EventReceiveHandler, we have to detect in UnsetReceiveHandler/Unsubscribe, whether we are in the
    ///          EventReceiveHandler call context or not and thus to call scope.expire() or not. This thread local
    ///          variable enables this detection.
    static thread_local bool is_in_receive_handler_context;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXY_EVENT_BASE_H
