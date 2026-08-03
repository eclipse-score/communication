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
#ifndef SCORE_MW_COM_IMPL_PROXYEVENT_H
#define SCORE_MW_COM_IMPL_PROXYEVENT_H

#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/proxy_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/proxy_base.h"
#include "score/mw/com/impl/proxy_event_base.h"
#include "score/mw/com/impl/proxy_event_binding.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/tracing/proxy_event_tracing.h"

#include "score/mw/com/impl/mocking/i_proxy_event.h"

#include "score/mw/log/logging.h"
#include "score/result/result.h"

#include <score/assert.hpp>

#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

// False Positive: this is a normal forward declaration.
// Which is used to avoid cyclic dependency with proxy_field.h
// coverity[autosar_cpp14_m3_2_3_violation]
template <typename, typename...>
class ProxyField;

/// \brief This is the user-visible class of an event that is part of a proxy. It contains ProxyEvent functionality that
/// requires knowledge of the SampleType. All type agnostic functionality is stored in the base class, ProxyEventBase.
///
/// The class itself is a concrete type. However, it delegates all actions to an implementation
/// that is provided by the binding the proxy is operating on.
///
/// \tparam SampleDataType Type of data that is transferred by the event.
template <typename SampleDataType>
class ProxyEvent final : public ProxyEventBase
{
    template <typename T>
    // Design decission: This friend class provides a view on the internals of ProxyEvent.
    // This enables us to hide unncecessary internals from the enduser.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyEventView;

    // Design decission: ProxyField uses composition pattern to reuse code from ProxyEvent. These two classes also have
    // shared private APIs which necessitates the use of the friend keyword.
    // coverity[autosar_cpp14_a11_3_1_violation]
    template <typename, typename...>
    friend class ProxyFieldImpl;

    // Empty struct that is used to make the second constructor only accessible to ProxyField (as it is a friend).
    struct FieldOnlyConstructorEnabler
    {
    };

  public:
    using SampleType = SampleDataType;

    /// Constructor which is dispatched to by the other public constructors.
    ///
    /// Instantiates the base class and members of ProxyEvent. Should only be called directly in tests.
    ///
    /// \param proxy_binding The binding that shall be associated with this proxy.
    ProxyEvent(const std::string_view event_name, std::unique_ptr<ProxyEventBinding<SampleType>> proxy_event_binding);

    /// \brief Constructor that allows to set the binding directly.
    ///
    /// This is used by ProxyField to pass in a ProxyEventBinding created using the ProxyFieldBindingFactory.
    ///
    /// \param proxy_binding The binding that shall be associated with this proxy.
    ProxyEvent(ProxyBase& base,
               const std::string_view event_name,
               Result<std::unique_ptr<ProxyEventBinding<SampleType>>> proxy_event_binding,
               FieldOnlyConstructorEnabler);

    /// \brief Constructs a ProxyEvent by querying the base proxie's ProxyBinding for the respective ProxyEventBinding.
    ///
    /// \param base Proxy that contains this event
    /// \param event_name Event name of the event, taken from the AUTOSAR model
    /// \todo Remove unneeded parameter once we get these information from the configuration
    ProxyEvent(ProxyBase& base, const std::string_view event_name);

    /// \brief A ProxyEvent shall not be copyable
    ProxyEvent(const ProxyEvent&) = delete;
    ProxyEvent& operator=(const ProxyEvent&) & = delete;

    /// \brief A ProxyEvent shall be movable
    ProxyEvent(ProxyEvent&& other) noexcept = default;
    ProxyEvent& operator=(ProxyEvent&& other) noexcept = default;

    ~ProxyEvent() override = default;

    /**
     * \api
     * \brief Receive pending data from the event.
     * \details The user needs to provide a callable that fulfills the following signature:
     *          void F(SamplePtr<const SampleType>) noexcept. This callback will be called for each sample
     *          that is available at the time of the call. Notice that the number of callback calls cannot
     *          exceed std::min(GetFreeSampleCount(), max_num_samples) times.
     * \tparam F Callable with the signature void(SamplePtr<const SampleType>) noexcept
     * \param receiver Callable with the appropriate signature. GetNewSamples will take ownership
     *                 of this callable.
     * \param max_num_samples Maximum number of samples to return via the given callable.
     * \return Number of samples that were handed over to the callable or an error.
     */
    template <typename F>
    Result<std::size_t> GetNewSamples(F&& receiver, std::size_t max_num_samples) noexcept;

    void InjectMock(IProxyEvent<SampleType>& proxy_event_mock)
    {
        proxy_event_mock_ = &proxy_event_mock;
        ProxyEventBase::InjectMock(proxy_event_mock);
    }

  private:
    ProxyEventBinding<SampleType>* GetTypedEventBinding() const noexcept;

    /// \brief Computes the typed event binding pointer from the type-erased binding_base_ (nullptr-check + downcast).
    ///
    /// This is called exactly once per ProxyEvent (during construction) and its result is cached in
    /// #typed_event_binding_ so that the hot path GetNewSamples() does not have to perform a dynamic_cast on every
    /// call.
    ProxyEventBinding<SampleType>* ComputeTypedEventBinding() const noexcept;

    IProxyEvent<SampleType>* proxy_event_mock_;

    /// \brief Cached typed event binding, computed once during construction. Points to the same object owned by
    /// ProxyEventBase::binding_base_ (or nullptr if the binding construction failed). Remains valid across moves since
    /// the defaulted move only transfers ownership of the underlying heap object without changing its address.
    ProxyEventBinding<SampleType>* typed_event_binding_;

    /// \brief Indicates whether this event is a field event (i.e. owned by a ProxyField, which is a composite of
    /// method/event) or not.
};

template <typename SampleType>
ProxyEvent<SampleType>::ProxyEvent(const std::string_view event_name,
                                   std::unique_ptr<ProxyEventBinding<SampleType>> proxy_event_binding)
    : ProxyEventBase{event_name, std::unique_ptr<ProxyEventBindingBase>{std::move(proxy_event_binding)}},
      proxy_event_mock_{nullptr},
      typed_event_binding_{ComputeTypedEventBinding()}
{
}

template <typename SampleType>
ProxyEvent<SampleType>::ProxyEvent(ProxyBase& base, const std::string_view event_name)

    : ProxyEventBase{event_name,
                     ProxyEventBindingFactory<SampleType>::Create(base.GetHandle(),
                                                                  ProxyBaseView{base}.GetBinding(),
                                                                  event_name,
                                                                  ServiceElementType::EVENT)
                         .and_then([](std::unique_ptr<ProxyEventBinding<SampleType>> binding) {
                             return Result<std::unique_ptr<ProxyEventBindingBase>>{std::move(binding)};
                         })},
      proxy_event_mock_{nullptr},
      typed_event_binding_{ComputeTypedEventBinding()}
{
    ProxyBaseView proxy_base_view{base};
    proxy_base_view.RegisterEvent(event_name, GetReferenceToMoveable());
    if (typed_event_binding_ != nullptr)
    {
        const auto& instance_identifier = proxy_base_view.GetAssociatedHandleType().GetInstanceIdentifier();
        tracing_data_ = tracing::GenerateProxyTracingStructFromEventConfig(instance_identifier, event_name);
    }
}

template <typename SampleType>
ProxyEvent<SampleType>::ProxyEvent(ProxyBase& base,
                                   const std::string_view event_name,
                                   Result<std::unique_ptr<ProxyEventBinding<SampleType>>> proxy_event_binding,
                                   FieldOnlyConstructorEnabler)
    : ProxyEventBase{event_name, std::move(proxy_event_binding)},
      proxy_event_mock_{nullptr},
      typed_event_binding_{ComputeTypedEventBinding()}
{
    // This is the specific ctor that is used by ProxyField for its "dispatch event" composite. Therefore, we do not
    // register the event in the ProxyBase's event map, since registration in the correct field map is done by
    // ProxyField ctor
    if (typed_event_binding_ != nullptr)
    {
        ProxyBaseView proxy_base_view{base};
        const auto& instance_identifier = proxy_base_view.GetAssociatedHandleType().GetInstanceIdentifier();
        tracing_data_ = tracing::GenerateProxyTracingStructFromFieldConfig(instance_identifier, event_name);
    }
}

template <typename SampleType>
template <typename F>
Result<std::size_t> ProxyEvent<SampleType>::GetNewSamples(F&& receiver, std::size_t max_num_samples) noexcept
{
    if (proxy_event_mock_ != nullptr)
    {
        typename IProxyEvent<SampleType>::Callback mock_callback =
            [receiver = std::forward<F>(receiver)](SamplePtr<SampleType> sample_ptr) noexcept {
                receiver(std::move(sample_ptr));
            };
        return proxy_event_mock_->GetNewSamples(std::move(mock_callback), max_num_samples);
    }

    tracing::TraceGetNewSamples(tracing_data_, *binding_base_);

    auto guard_factory = tracker_->Allocate(max_num_samples);

    if (guard_factory.GetNumAvailableGuards() == 0U)
    {
        score::mw::log::LogWarn("lola")
            << "Unable to emit new samples, no free sample slots for this subscription available.";
        return MakeUnexpected(ComErrc::kMaxSamplesReached);
    }

    auto tracing_receiver = tracing::CreateTracingGetNewSamplesCallback<SampleType, F>(
        tracing_data_, *binding_base_, std::forward<F>(receiver));

    const auto get_new_samples_result = typed_event_binding_->GetNewSamples(std::move(tracing_receiver), guard_factory);
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

template <typename SampleType>
auto ProxyEvent<SampleType>::GetTypedEventBinding() const noexcept -> ProxyEventBinding<SampleType>*
{
    return typed_event_binding_;
}

template <typename SampleType>
auto ProxyEvent<SampleType>::ComputeTypedEventBinding() const noexcept -> ProxyEventBinding<SampleType>*
{
    if (binding_base_ == nullptr)
    {
        return nullptr;
    }
    auto typed_binding = dynamic_cast<ProxyEventBinding<SampleType>*>(binding_base_.get());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(typed_binding != nullptr, "Downcast to ProxyEventBinding failed!");
    return typed_binding;
}

template <typename SampleType>
class ProxyEventView
{
  public:
    explicit ProxyEventView(ProxyEvent<SampleType>& proxy_event) : proxy_event_{proxy_event} {}

    ProxyEventBinding<SampleType>* GetBinding() const noexcept
    {
        return proxy_event_.GetTypedEventBinding();
    }

  private:
    ProxyEvent<SampleType>& proxy_event_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXYEVENT_H
