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

#include "score/mw/com/impl/field_tags.h"
#include "score/mw/com/impl/methods/method_signature_element_ptr.h"
#include "score/mw/com/impl/methods/proxy_method_with_in_args_and_return.h"
#include "score/mw/com/impl/methods/proxy_method_with_return_type.h"
#include "score/mw/com/impl/plumbing/proxy_field_binding_factory.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/proxy_event_binding.h"
#include "score/mw/com/impl/proxy_field_base.h"

#include "score/mw/com/impl/methods/proxy_method_base.h"

#include "score/mw/com/impl/mocking/i_proxy_event.h"

#include "score/result/result.h"

#include <score/assert.hpp>

#include <cstddef>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace score::mw::com::impl
{

// Suppress "AUTOSAR C++14 M3-2-3" rule finding. This rule states: "An identifier with external linkage shall have
// exactly one definition". This a forward class declaration.
// coverity[autosar_cpp14_m3_2_3_violation]
template <typename FieldType, typename... Tags>
class ProxyFieldAttorney;

/// \brief This is the user-visible class of a field that is part of a proxy. It delegates all functionality to
/// ProxyEvent.
///
/// \tparam SampleDataType Type of data carried by the field.
/// \tparam Tags Pack of marker tags from field_tags.h. Any combination of WithGetter, WithSetter, WithNotifier is
///         supported, subject to the static_assert below: at least one of WithGetter or WithNotifier must be
///         present (otherwise the value is invisible to consumers). WithGetter enables Get(), WithSetter enables
///         Set(), WithNotifier enables the notifier methods Subscribe(), Unsubscribe(), GetSubscriptionState(),
///         GetFreeSampleCount(), GetNumNewSamplesAvailable(), SetReceiveHandler(), UnsetReceiveHandler(),
///         GetNewSamples().
template <typename SampleDataType, typename... Tags>
class ProxyFieldImpl : public ProxyFieldBase
{

    // A ProxyFieldImpl must enable WithGetter or WithNotifier. A field with only WithSetter (or no tags at all)
    // gives the consumer no way to observe the value it can write, making the field useless.
    static_assert(
        contains_type<WithNotifier, Tags...>::value || contains_type<WithGetter, Tags...>::value,
        "A field must either have WithGetter or WithNotifier enabled otherwise, there is no way for the consumer to "
        "receive the field values.");

    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // Design decision: The "*Attorney" class is a helper, which sets the internal state of this class accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyFieldAttorney<SampleDataType, Tags...>;

  public:
    using FieldType = SampleDataType;

    /// \brief Testing ctor that delegates to the production ctor that accepts mock bindings directly, this can only be
    /// used in test code.
    /// \details The template tags do not gate this overload; they continue to control which parts of the public
    ///          API (Get, Set, Subscribe, ...) are available on the resulting object. The event binding parameter
    ///          is required (non default) to disambiguate this overload from the production ctor.
    /// \param proxy_base Parent proxy that owns this field's registration.
    /// \param field_name Field's name as it appears in the deployment.
    /// \param event_binding Mock event binding. Must be provided (use nullptr if no event binding is needed).
    /// \param set_method_binding Optional mock Set-method binding. If nullptr, no ProxyMethod is built for Set.
    /// \param get_method_binding Optional mock Get-method binding. If nullptr, no ProxyMethod is built for Get.
    ProxyFieldImpl(const std::string_view field_name,
                   std::unique_ptr<ProxyEventBinding<FieldType>> event_binding,
                   std::unique_ptr<ProxyMethodBinding> set_method_binding = nullptr,
                   std::unique_ptr<ProxyMethodBinding> get_method_binding = nullptr)
        : ProxyFieldImpl{field_name,
                         std::make_unique<ProxyEvent<FieldType>>(field_name, std::move(event_binding)),
                         set_method_binding == nullptr
                             ? nullptr
                             : std::make_unique<ProxyMethod<SetMethodSignature>>(
                                   field_name,
                                   std::move(set_method_binding),
                                   typename ProxyMethod<SetMethodSignature>::FieldSetterConstructorEnabler{}),
                         get_method_binding == nullptr
                             ? nullptr
                             : std::make_unique<ProxyMethod<GetMethodSignature>>(
                                   field_name,
                                   std::move(get_method_binding),
                                   typename ProxyMethod<GetMethodSignature>::FieldGetterConstructorEnabler{})}
    {
    }

    /// \brief Production ctor used by the end users.
    /// \details The Make*IfEnabled helpers consult the tag pack at compile time and return nullptr when their
    ///          corresponding tag is absent, so the same body produces the correct dispatch for every valid
    ///          tag combination. The class-level static_assert ensures that at least one of WithGetter or
    ///          WithNotifier is present.
    /// \param proxy_base Parent proxy that owns this field's registration.
    /// \param field_name Field's name as it appears in the deployment.
    ProxyFieldImpl(ProxyBase& proxy_base, const std::string_view field_name)
        : ProxyFieldImpl{field_name,
                         MakeEventDispatchIfEnabled(proxy_base, field_name),
                         MakeSetMethodDispatchIfEnabled(proxy_base, field_name),
                         MakeGetMethodDispatchIfEnabled(proxy_base, field_name)}
    {
        // Each Make*IfEnabled must have produced a non-null dispatch when the
        // corresponding tag is in the pack.
        if constexpr (kHasNotifier)
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(proxy_event_dispatch_ != nullptr);
        }
        if constexpr (kHasSetter)
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(proxy_method_set_dispatch_ != nullptr);
        }
        if constexpr (kHasGetter)
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(proxy_method_get_dispatch_ != nullptr);
        }
        ProxyBaseView proxy_base_view{proxy_base};
        proxy_base_view.RegisterField(field_name, GetReferenceToMoveable());
    }

    /// \brief A ProxyFieldImpl shall not be copyable
    ProxyFieldImpl(const ProxyFieldImpl&) = delete;
    ProxyFieldImpl& operator=(const ProxyFieldImpl&) = delete;

    /// \brief A ProxyField shall be moveable
    ProxyFieldImpl(ProxyFieldImpl&&) noexcept = default;
    ProxyFieldImpl& operator=(ProxyFieldImpl&&) noexcept = default;

    ~ProxyFieldImpl() noexcept = default;

    /**
     * \api
     * \brief Receive pending data from the field.
     * \details The user needs to provide a callable that fulfills the following signature:
     *          void(SamplePtr<const FieldType>) noexcept. This callback will be called for each sample
     *          that is available at the time of the call. Notice that the number of callback calls cannot
     *          exceed std::min(GetFreeSampleCount(), max_num_samples) times.
     * \tparam ReceiverType Callable with the signature void(SamplePtr<const FieldType>) noexcept
     * \param receiver Callable with the appropriate signature. GetNewSamples will take ownership
     *                 of this callable.
     * \param max_num_samples Maximum number of samples to return via the given callable.
     * \return Number of samples that were handed over to the callable or an error.
     */
    template <typename ReceiverType,
              typename T = SampleDataType,
              typename = std::enable_if_t<is_tag_enabled<T, SampleDataType, WithNotifier, Tags...>::value>>
    Result<std::size_t> GetNewSamples(ReceiverType&& receiver, const std::size_t max_num_samples) noexcept
    {
        return proxy_event_dispatch_->GetNewSamples(std::forward<ReceiverType>(receiver), max_num_samples);
    }

    /**
     * \api
     * \brief Subscribe to the field.
     * \param max_sample_count Specify the maximum number of concurrent samples that this event shall
     *                          be able to offer to the using application.
     * \return On failure, returns an error code.
     */
    template <typename T = SampleDataType,
              typename = std::enable_if_t<is_tag_enabled<T, SampleDataType, WithNotifier, Tags...>::value>>
    Result<void> Subscribe(const std::size_t max_sample_count) noexcept
    {
        return ProxyFieldBase::Subscribe(max_sample_count);
    }

    /**
     * \api
     * \brief End subscription to a field and release needed resources.
     * \details It is illegal to call this method while data is still held by the application in the form of SamplePtr.
     *          Doing so will result in undefined behavior. After a call to this method, the field behaves as if it had
     *          just been constructed.
     */
    template <typename T = SampleDataType,
              typename = std::enable_if_t<is_tag_enabled<T, SampleDataType, WithNotifier, Tags...>::value>>
    void Unsubscribe() noexcept
    {
        ProxyFieldBase::Unsubscribe();
    }

    /**
     * \api
     * \brief Get the subscription state of this field.
     * \details This method can always be called regardless of the state of the field.
     * \return Subscription state of the field.
     */
    template <typename T = SampleDataType,
              typename = std::enable_if_t<is_tag_enabled<T, SampleDataType, WithNotifier, Tags...>::value>>
    SubscriptionState GetSubscriptionState() noexcept
    {
        return ProxyFieldBase::GetSubscriptionState();
    }

    /**
     * \api
     * \brief Get the number of samples that can still be received by the user of this field.
     * \details If this returns 0, the user first has to drop at least one SamplePtr before it is possible to receive
     *          data via GetNewSamples again. If there is no subscription for this field, the returned value is
     *          unspecified.
     * \return Number of samples that can still be received.
     */
    template <typename T = SampleDataType,
              typename = std::enable_if_t<is_tag_enabled<T, SampleDataType, WithNotifier, Tags...>::value>>
    std::size_t GetFreeSampleCount() noexcept
    {
        return ProxyFieldBase::GetFreeSampleCount();
    }

    /**
     * \api
     * \brief Returns the number of new samples a call to GetNewSamples() (given parameter max_num_samples
     *        doesn't restrict it) would currently provide.
     * \details This is a proprietary extension to the official ara::com API. It is useful in resource sensitive
     *          setups, where the user wants to work in polling mode only without registered async receive-handlers.
     *          For further details see //score/mw/com/design/extensions/README.md.
     * \return Either 0 if no new samples are available (and GetNewSamples() wouldn't return any) or N, where 1 <= N <=
     *         actual new samples. I.e. an implementation is allowed to report a lower number than actual new samples,
     *         which would be provided by a call to GetNewSamples().
     */
    template <typename T = SampleDataType,
              typename = std::enable_if_t<is_tag_enabled<T, SampleDataType, WithNotifier, Tags...>::value>>
    Result<std::size_t> GetNumNewSamplesAvailable() noexcept
    {
        return ProxyFieldBase::GetNumNewSamplesAvailable();
    }

    /**
     * \api
     * \brief Sets the handler to be called, whenever a new field value has been received.
     * \details Generally a ReceiveHandler has no restrictions on what mw::com API it is allowed to call.
     *          It is especially allowed to call all public APIs of the Field instance on which it had been
     *          set/registered as long as it obeys the general requirement, that API calls on a Proxy/Proxy field are
     *          thread safe/can't be called concurrently.
     * \attention This function MUST NOT be called from the context of a ReceiveHandler registered for this field!
     *            It makes semantically not really sense to register a "new" ReceiveHandler from the context of an
     *            already running ReceiveHandler. We also see no use cases for it and won't support it therefore.
     * \param handler user provided handler to be called
     */
    template <typename T = SampleDataType,
              typename = std::enable_if_t<is_tag_enabled<T, SampleDataType, WithNotifier, Tags...>::value>>
    Result<void> SetReceiveHandler(EventReceiveHandler handler) noexcept
    {
        return ProxyFieldBase::SetReceiveHandler(std::move(handler));
    }

    /**
     * \api
     * \brief Unsets/Unregisters a SubscriptionStateChangeHandler for this field. After this method returns, it is
     *        guaranteed, that the previously registered handler is neither active nor will be called anymore.
     */
    template <typename T = SampleDataType,
              typename = std::enable_if_t<is_tag_enabled<T, SampleDataType, WithNotifier, Tags...>::value>>
    Result<void> UnsetSubscriptionStateChangeHandler() noexcept
    {
        return ProxyFieldBase::UnsetSubscriptionStateChangeHandler();
    }

    /**
     * \api
     * \brief Sets/Registers a SubscriptionStateChangeHandler for this event. This handler will be called whenever the
     * subscription state of this event changes.
     * \note An already set/registered SubscriptionStateChangeHandler will be silently overridden.
     * \param handler
     */
    template <typename T = SampleDataType,
              typename = std::enable_if_t<is_tag_enabled<T, SampleDataType, WithNotifier, Tags...>::value>>
    Result<void> SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler handler) noexcept
    {
        return ProxyFieldBase::SetSubscriptionStateChangeHandler(std::move(handler));
    }

    /**
     * \api
     * \brief Removes any ReceiveHandler registered via SetReceiveHandler.
     */
    template <typename T = SampleDataType,
              typename = std::enable_if_t<is_tag_enabled<T, SampleDataType, WithNotifier, Tags...>::value>>
    Result<void> UnsetReceiveHandler() noexcept
    {
        return ProxyFieldBase::UnsetReceiveHandler();
    }

    /**
     * \api
     * \brief Get the current value of the field.
     * \details This method is only available when the field is created with the WithGetter tag
     */
    template <typename T = SampleDataType,
              typename = std::enable_if_t<is_tag_enabled<T, SampleDataType, WithGetter, Tags...>::value>>
    score::Result<MethodReturnTypePtr<T>> Get() noexcept
    {
        return proxy_method_get_dispatch_->operator()();
    }

    /**
     * \api
     * \brief Set a new value to the field.
     * \details This method is only available when the field is created with the WithSetter tag
     */
    template <typename T = SampleDataType,
              typename = std::enable_if_t<is_tag_enabled<T, SampleDataType, WithSetter, Tags...>::value>>
    score::Result<MethodReturnTypePtr<T>> Set(const SampleDataType& new_field_value) noexcept
    {
        // If the method call itself fails, then we return the error code to the user here.
        auto method_call_result = proxy_method_set_dispatch_->operator()(new_field_value);
        if (!method_call_result.has_value())
        {
            return MakeUnexpected<MethodReturnTypePtr<T>>(std::move(method_call_result).error());
        }

        // If the method call itself succeeded but executing the setter itself on skeleton side failed (e.g. because
        // updating the field value failed), then we return the error code to the user as well.
        auto setter_return_type_ptr = std::move(method_call_result).value();
        if (!setter_return_type_ptr->has_value())
        {
            return MakeUnexpected<MethodReturnTypePtr<T>>(std::move(*setter_return_type_ptr).error());
        }

        // If the method call and executing the setter itself succeeded, then we return a pointer to the set value to
        // the user.
        T& value_ptr = setter_return_type_ptr->value();

        MethodReturnTypePtr<T> return_value{value_ptr, std::move(setter_return_type_ptr)};

        return return_value;
    }

    template <typename T = SampleDataType,
              typename = std::enable_if_t<is_tag_enabled<T, SampleDataType, WithNotifier, Tags...>::value>>
    void InjectMock(IProxyEvent<FieldType>& proxy_event_mock)
    {
        proxy_event_dispatch_->InjectMock(proxy_event_mock);
    }

  private:
    /// \brief Signatures of the internal Set and Get methods.
    ///
    /// Since the setter / getter functions can fail within the middleware code (e.g. when trying to update the field
    /// value fails), we need to return a result from the setter and getter.
    using SetMethodSignature = score::Result<FieldType>(FieldType);
    using GetMethodSignature = FieldType();

    static constexpr bool kHasNotifier = contains_type<WithNotifier, Tags...>::value;
    static constexpr bool kHasSetter = contains_type<WithSetter, Tags...>::value;
    static constexpr bool kHasGetter = contains_type<WithGetter, Tags...>::value;

    /// \brief Builds the proxy event dispatch via the binding factory when WithNotifier is enabled.
    /// \return A valid ProxyEvent dispatch when WithNotifier is in the tag pack, nullptr otherwise.
    static std::unique_ptr<ProxyEvent<FieldType>> MakeEventDispatchIfEnabled(ProxyBase& proxy_base,
                                                                             const std::string_view field_name)
    {
        if constexpr (kHasNotifier)
        {
            return std::make_unique<ProxyEvent<FieldType>>(
                proxy_base,
                field_name,
                ProxyFieldBindingFactory<FieldType>::CreateEventBinding(
                    proxy_base.GetHandle(), ProxyBaseView{proxy_base}.GetBinding(), field_name),
                typename ProxyEvent<FieldType>::FieldOnlyConstructorEnabler{});
        }
        else
        {
            std::ignore = proxy_base;
            std::ignore = field_name;
            return nullptr;
        }
    }

    /// \brief Builds the Set-method dispatch via the binding factory when WithSetter is enabled.
    /// \return A valid ProxyMethod dispatch when WithSetter is in the tag pack, nullptr otherwise.
    static std::unique_ptr<ProxyMethod<SetMethodSignature>> MakeSetMethodDispatchIfEnabled(
        ProxyBase& proxy_base,
        const std::string_view field_name)
    {
        if constexpr (kHasSetter)
        {
            return std::make_unique<ProxyMethod<SetMethodSignature>>(
                field_name,
                ProxyFieldBindingFactory<FieldType>::CreateSetMethodBinding(
                    proxy_base.GetHandle(), ProxyBaseView{proxy_base}.GetBinding(), field_name),
                typename ProxyMethod<SetMethodSignature>::FieldSetterConstructorEnabler{});
        }
        else
        {
            std::ignore = proxy_base;
            std::ignore = field_name;
            return nullptr;
        }
    }

    /// \brief Builds the Get-method dispatch via the binding factory when WithGetter is enabled.
    /// \return A valid ProxyMethod dispatch when WithGetter is in the tag pack, nullptr otherwise.
    static std::unique_ptr<ProxyMethod<GetMethodSignature>> MakeGetMethodDispatchIfEnabled(
        ProxyBase& proxy_base,
        const std::string_view field_name)
    {
        if constexpr (kHasGetter)
        {
            return std::make_unique<ProxyMethod<GetMethodSignature>>(
                field_name,
                ProxyFieldBindingFactory<FieldType>::CreateGetMethodBinding(
                    proxy_base.GetHandle(), ProxyBaseView{proxy_base}.GetBinding(), field_name),
                typename ProxyMethod<GetMethodSignature>::FieldGetterConstructorEnabler{});
        }
        else
        {
            std::ignore = proxy_base;
            std::ignore = field_name;
            return nullptr;
        }
    }

    ProxyFieldImpl(const std::string_view field_name,
                   std::unique_ptr<ProxyEvent<FieldType>> proxy_event_dispatch,
                   std::unique_ptr<ProxyMethod<SetMethodSignature>> proxy_method_set_dispatch,
                   std::unique_ptr<ProxyMethod<GetMethodSignature>> proxy_method_get_dispatch)
        : ProxyFieldBase{field_name,
                         proxy_event_dispatch.get(),
                         proxy_method_set_dispatch.get(),
                         proxy_method_get_dispatch.get()},
          proxy_event_dispatch_{std::move(proxy_event_dispatch)},
          proxy_method_set_dispatch_{std::move(proxy_method_set_dispatch)},
          proxy_method_get_dispatch_{std::move(proxy_method_get_dispatch)}
    {
    }

    std::unique_ptr<ProxyEvent<FieldType>> proxy_event_dispatch_;

    std::unique_ptr<ProxyMethod<SetMethodSignature>> proxy_method_set_dispatch_;
    std::unique_ptr<ProxyMethod<GetMethodSignature>> proxy_method_get_dispatch_;

    static_assert(
        std::is_same_v<decltype(proxy_event_dispatch_), std::unique_ptr<ProxyEvent<FieldType>>>,
        "proxy_event_dispatch_ needs to be a unique_ptr since we pass a pointer to it to ProxyFieldBase, so "
        "we must ensure that it doesn't move when the ProxyFieldImpl is moved to avoid dangling references. ");
};

template <typename SampleDataType, typename... Tags>
class ProxyField final : public ProxyFieldImpl<SampleDataType, Tags...>
{
    using ProxyFieldImpl<SampleDataType, Tags...>::ProxyFieldImpl;
};

template <typename SampleDataType>
class [[deprecated("Implicit deduction  of field semantics is deprecated. Choose at least WithNotifier or WithGetter")]]
ProxyField<SampleDataType>
    final : public ProxyFieldImpl<SampleDataType, WithNotifier>
{
    using ProxyFieldImpl<SampleDataType, WithNotifier>::ProxyFieldImpl;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXY_FIELD_H
