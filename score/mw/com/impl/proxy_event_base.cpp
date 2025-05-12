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
#include "score/mw/com/impl/proxy_event_base.h"

#include "score/language/safecpp/scoped_function/scope.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/scoped_event_receive_handler.h"
#include "score/mw/com/impl/tracing/proxy_event_tracing.h"

#include "score/result/result.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <exception>
#include <memory>
#include <utility>

namespace score::mw::com::impl
{

// Initialization of static thread_local variable!
thread_local bool ProxyEventBase::is_in_receive_handler_context = false;

/// \brief Helper class which registers the ProxyEventBase with its parent proxy and unregisters on destruction
///
/// Since ProxyBase is moveable, we must ensure that this class does not store a reference or pointer to it. As if the
/// Proxy is moved, then the pointer or reference would be invalidated. However, the ProxyBinding is not moveable, so
/// storing a pointer to the ProxyBinding is safe.
class EventBindingRegistrationGuard final
{
  public:
    EventBindingRegistrationGuard(ProxyBase& proxy_base,
                                  ProxyEventBindingBase* proxy_event_binding_base,
                                  score::cpp::string_view event_name) noexcept
        : proxy_binding_{ProxyBaseView{proxy_base}.GetBinding()},
          proxy_event_binding_base_{proxy_event_binding_base},
          event_name_{event_name}
    {
        if (proxy_event_binding_base_ == nullptr)
        {
            ProxyBaseView{proxy_base}.MarkServiceElementBindingInvalid();
            return;
        }
        if (proxy_binding_ != nullptr)
        {
            proxy_binding_->RegisterEventBinding(event_name, *proxy_event_binding_base);
        }
    }

    ~EventBindingRegistrationGuard() noexcept
    {
        if ((proxy_binding_ != nullptr) && (proxy_event_binding_base_ != nullptr))
        {
            proxy_binding_->UnregisterEventBinding(event_name_);
        }
    }

    EventBindingRegistrationGuard(const EventBindingRegistrationGuard&) = delete;
    EventBindingRegistrationGuard& operator=(const EventBindingRegistrationGuard&) = delete;
    EventBindingRegistrationGuard(EventBindingRegistrationGuard&& other) = delete;
    EventBindingRegistrationGuard& operator=(EventBindingRegistrationGuard&& other) = delete;

  private:
    ProxyBinding* proxy_binding_;
    ProxyEventBindingBase* proxy_event_binding_base_;
    score::cpp::string_view event_name_;
};

// Suppress "AUTOSAR C++14 A3-1-1", The rule states: "It shall be possible to include any header file
// in multiple translation units without violating the One Definition Rule."
// This is false positive. Function is declared only once.
// coverity[autosar_cpp14_a3_1_1_violation]
ProxyEventBase::ProxyEventBase(ProxyBase& proxy_base,
                               std::unique_ptr<ProxyEventBindingBase> proxy_event_binding,
                               score::cpp::string_view event_name) noexcept
    : binding_base_{std::move(proxy_event_binding)},
      tracker_{std::make_unique<SampleReferenceTracker>()},
      tracing_data_{},
      event_binding_registration_guard_{
          std::make_unique<EventBindingRegistrationGuard>(proxy_base, binding_base_.get(), event_name)},
      receive_handler_scope_{}
{
}

ProxyEventBase::~ProxyEventBase() noexcept
{
    // If the ProxyEventBase has been moved, then tracker_ will be a nullptr
    if (tracker_ != nullptr && tracker_->IsUsed())
    {
        score::mw::log::LogFatal("lola")
            << "Proxy event instance destroyed while still holding SamplePtr instances, terminating.";
        std::terminate();
    }
}

ProxyEventBase::ProxyEventBase(ProxyEventBase&&) noexcept = default;
ProxyEventBase& ProxyEventBase::operator=(ProxyEventBase&&) noexcept = default;

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from '.value()' in case the returned
// result doesn't have value but as we check before with 'has_value()' so no way for throwing std::bad_optional_access
// which leds to std::terminate().
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
ResultBlank ProxyEventBase::Subscribe(const std::size_t max_sample_count) noexcept
{
    tracing::TraceSubscribe(tracing_data_, *binding_base_, max_sample_count);

    const auto current_state = GetSubscriptionState();
    if (current_state == SubscriptionState::kNotSubscribed)
    {
        tracker_->Reset(max_sample_count);
        const auto subscribe_result = binding_base_->Subscribe(max_sample_count);
        if (!subscribe_result.has_value())
        {
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
    }
    else if ((current_state == SubscriptionState::kSubscribed) ||
             (current_state == SubscriptionState::kSubscriptionPending))
    {
        const auto current_max_sample_count = binding_base_->GetMaxSampleCount();
        SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(current_max_sample_count.has_value(), "Current MaxSampleCount must be set when subscribed.");
        if (max_sample_count != current_max_sample_count.value())
        {
            return MakeUnexpected(ComErrc::kMaxSampleCountNotRealizable);
        }
    }
    return {};
}

void ProxyEventBase::Unsubscribe() noexcept
{
    tracing::TraceUnsubscribe(tracing_data_, *binding_base_);

    if (GetSubscriptionState() != SubscriptionState::kNotSubscribed)
    {
        // before actually unsubscribing, we have to sync first with any concurrently running ReceiveHandler:
        // ReceiveHandler will be implicitly unset during Unsubscribe and therefore any current invocation has to finish
        // first, which we assure via its scope expiring.
        ExpireReceiveHandlerScopeIfNotInHandler();
        binding_base_->Unsubscribe();
        if (tracker_->IsUsed())
        {
            score::mw::log::LogFatal("lola")
                << "Called unsubscribe while still holding SamplePtr instances, terminating.";
            std::terminate();
        }
    }
}

Result<std::size_t> ProxyEventBase::GetNumNewSamplesAvailable() const noexcept
{
    const auto get_num_new_samples_available_result = binding_base_->GetNumNewSamplesAvailable();
    if (!get_num_new_samples_available_result.has_value())
    {
        if (get_num_new_samples_available_result.error() == ComErrc::kNotSubscribed)
        {
            return get_num_new_samples_available_result;
        }
        else
        {
            return MakeUnexpected(ComErrc::kBindingFailure);
        }
    }
    return get_num_new_samples_available_result;
}

ResultBlank ProxyEventBase::SetReceiveHandler(EventReceiveHandler handler) noexcept
{
    tracing::TraceSetReceiveHandler(tracing_data_, *binding_base_);
    auto tracing_handler = tracing::CreateTracingReceiveHandler(tracing_data_, *binding_base_, std::move(handler));

    // Package the tracing handler, which already encapsulates the user provided EventReceiveHandler into another
    // move-only function, which adds the aspect of updating the thread local state is_in_receive_handler_context
    // correctly.
    auto extended_tracing_handler = [handler = std::move(tracing_handler)]() noexcept {
        is_in_receive_handler_context = true;
        handler();
        is_in_receive_handler_context = false;
    };

    // Create a new scope for the provided callable. This will also expire the scope of any previously registered
    // callable.
    receive_handler_scope_ = safecpp::Scope<>{};
    receive_handler_ptr_ =
        std::make_shared<ScopedEventReceiveHandler>(receive_handler_scope_, std::move(extended_tracing_handler));

    const auto set_receive_handler_result = binding_base_->SetReceiveHandler(receive_handler_ptr_);
    if (!set_receive_handler_result.has_value())
    {
        return MakeUnexpected(ComErrc::kSetHandlerNotSet);
    }
    return {};
}

ResultBlank ProxyEventBase::UnsetReceiveHandler() noexcept
{
    if (!receive_handler_ptr_)
    {
        // quick return in case no receive handler has been registered. As per API spec, we are nice to the user and
        // silently ignore this call.
        return {};
    }
    tracing::TraceUnsetReceiveHandler(tracing_data_, *binding_base_);

    ExpireReceiveHandlerScopeIfNotInHandler();
    receive_handler_ptr_.reset();

    const auto unset_receive_handler_result = binding_base_->UnsetReceiveHandler();
    if (!unset_receive_handler_result.has_value())
    {
        return MakeUnexpected(ComErrc::kUnsetFailure);
    }
    return {};
}

void ProxyEventBase::ExpireReceiveHandlerScopeIfNotInHandler()
{
    if (!is_in_receive_handler_context)
    {
        receive_handler_scope_.Expire();
    }
}

}  // namespace score::mw::com::impl
