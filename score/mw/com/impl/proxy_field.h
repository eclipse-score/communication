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

#include "score/mw/com/impl/methods/proxy_method_with_in_args_and_return.h"
#include "score/mw/com/impl/methods/proxy_method_with_return_type.h"
#include "score/mw/com/impl/plumbing/proxy_field_binding_factory.h"
#include "score/mw/com/impl/plumbing/sample_ptr.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/proxy_event_binding.h"
#include "score/mw/com/impl/proxy_field_base.h"

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
template <typename FieldType>
class ProxyFieldAttorney;

/// \brief This is the user-visible class of a field that is part of a proxy. It delegates all functionality to
/// ProxyEvent.
///
/// \tparam SampleDataType Type of data that is transferred by the event.
/// \tparam EnableGet Whether the get method shall be enabled for this field. If true, a Get() method will be available.
/// \tparam EnableSet Whether the set method shall be enabled for this field. If true, a Set() method will be available.
/// \tparam EnableNotifier Whether the notifier functionality shall be enabled for this field. Whether this has an
/// effect, depends on the binding that is used. The LoLa/shm-binding ignores this template parameter.
template <typename SampleDataType,
          const bool EnableGet = false,
          const bool EnableSet = false,
          const bool EnableNotifier = false>
class ProxyField final : public ProxyFieldBase
{
    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // Design decision: The "*Attorney" class is a helper, which sets the internal state of this class accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class ProxyFieldAttorney<SampleDataType>;

  public:
    using FieldType = SampleDataType;

    /// Constructor that allows to set the binding directly (both EnableGet and EnableSet are true).
    ///
    /// This is used for testing only. Allows for directly setting the bindings, and usually the mock binding is used
    /// here.
    template <bool EG = EnableGet, bool ES = EnableSet, typename = std::enable_if_t<EG && ES>>
    ProxyField(ProxyBase& proxy_base,
               std::unique_ptr<ProxyEventBinding<FieldType>> event_binding,
               std::unique_ptr<ProxyMethodBinding> get_method_binding,
               std::unique_ptr<ProxyMethodBinding> set_method_binding,
               const std::string_view field_name,
               detail::EnableBothTag = {})
        : ProxyField{proxy_base,
                     std::make_unique<ProxyEvent<FieldType>>(proxy_base, std::move(event_binding), field_name),
                     std::make_unique<ProxyMethod<FieldType()>>(
                         proxy_base,
                         std::move(get_method_binding),
                         field_name,
                         typename ProxyMethod<FieldType()>::FieldOnlyConstructorEnabler{}),
                     std::make_unique<ProxyMethod<FieldType(FieldType)>>(
                         proxy_base,
                         std::move(set_method_binding),
                         field_name,
                         typename ProxyMethod<FieldType(FieldType)>::FieldOnlyConstructorEnabler{}),
                     field_name}
    {
    }

    /// Constructor that allows to set the binding directly (only EnableGet is true).
    ///
    /// This is used for testing only. Allows for directly setting the bindings, and usually the mock binding is used
    /// here.
    template <bool EG = EnableGet, bool ES = EnableSet, typename = std::enable_if_t<EG && !ES>>
    ProxyField(ProxyBase& proxy_base,
               std::unique_ptr<ProxyEventBinding<FieldType>> event_binding,
               std::unique_ptr<ProxyMethodBinding> get_method_binding,
               const std::string_view field_name,
               detail::EnableGetOnlyTag = {})
        : ProxyField{proxy_base,
                     std::make_unique<ProxyEvent<FieldType>>(proxy_base, std::move(event_binding), field_name),
                     std::make_unique<ProxyMethod<FieldType()>>(
                         proxy_base,
                         std::move(get_method_binding),
                         field_name,
                         typename ProxyMethod<FieldType()>::FieldOnlyConstructorEnabler{}),
                     field_name}
    {
    }

    /// Constructor that allows to set the binding directly (only EnableSet is true).
    ///
    /// This is used for testing only. Allows for directly setting the bindings, and usually the mock binding is used
    /// here.
    template <bool EG = EnableGet, bool ES = EnableSet, typename = std::enable_if_t<!EG && ES>>
    ProxyField(ProxyBase& proxy_base,
               std::unique_ptr<ProxyEventBinding<FieldType>> event_binding,
               std::unique_ptr<ProxyMethodBinding> set_method_binding,
               const std::string_view field_name,
               detail::EnableSetOnlyTag = {})
        : ProxyField{proxy_base,
                     std::make_unique<ProxyEvent<FieldType>>(proxy_base, std::move(event_binding), field_name),
                     std::make_unique<ProxyMethod<FieldType(FieldType)>>(
                         proxy_base,
                         std::move(set_method_binding),
                         field_name,
                         typename ProxyMethod<FieldType(FieldType)>::FieldOnlyConstructorEnabler{}),
                     field_name}
    {
    }

    /// Constructor that allows to set the binding directly (both EnableGet and EnableSet are false).
    ///
    /// This is used for testing only. Allows for directly setting the bindings, and usually the mock binding is used
    /// here.
    template <bool EG = EnableGet, bool ES = EnableSet, typename = std::enable_if_t<!EG && !ES>>
    ProxyField(ProxyBase& proxy_base,
               std::unique_ptr<ProxyEventBinding<FieldType>> event_binding,
               const std::string_view field_name,
               detail::EnableNeitherTag = {})
        : ProxyField{proxy_base,
                     std::make_unique<ProxyEvent<FieldType>>(proxy_base, std::move(event_binding), field_name),
                     field_name}
    {
    }

    /// \brief Constructs a ProxyField (both EnableGet and EnableSet are true). Normal ctor that is used in production
    /// code.
    template <bool EG = EnableGet, bool ES = EnableSet, typename = std::enable_if_t<EG && ES>>
    ProxyField(ProxyBase& proxy_base, const std::string_view field_name, detail::EnableBothTag = {})
        : ProxyField{proxy_base,
                     std::make_unique<ProxyEvent<FieldType>>(
                         proxy_base,
                         ProxyFieldBindingFactory<FieldType>::CreateEventBinding(proxy_base, field_name),
                         field_name,
                         typename ProxyEvent<FieldType>::FieldOnlyConstructorEnabler{}),
                     std::make_unique<ProxyMethod<FieldType()>>(
                         proxy_base,
                         ProxyFieldBindingFactory<FieldType>::CreateGetMethodBinding(proxy_base, field_name),
                         field_name,
                         typename ProxyMethod<FieldType()>::FieldOnlyConstructorEnabler{}),
                     std::make_unique<ProxyMethod<FieldType(FieldType)>>(
                         proxy_base,
                         ProxyFieldBindingFactory<FieldType>::CreateSetMethodBinding(proxy_base, field_name),
                         field_name,
                         typename ProxyMethod<FieldType(FieldType)>::FieldOnlyConstructorEnabler{}),
                     field_name}
    {
    }

    /// \brief Constructs a ProxyField (only EnableGet is true). Normal ctor that is used in production code.
    template <bool EG = EnableGet, bool ES = EnableSet, typename = std::enable_if_t<EG && !ES>>
    ProxyField(ProxyBase& proxy_base, const std::string_view field_name, detail::EnableGetOnlyTag = {})
        : ProxyField{proxy_base,
                     std::make_unique<ProxyEvent<FieldType>>(
                         proxy_base,
                         ProxyFieldBindingFactory<FieldType>::CreateEventBinding(proxy_base, field_name),
                         field_name,
                         typename ProxyEvent<FieldType>::FieldOnlyConstructorEnabler{}),
                     std::make_unique<ProxyMethod<FieldType()>>(
                         proxy_base,
                         ProxyFieldBindingFactory<FieldType>::CreateGetMethodBinding(proxy_base, field_name),
                         field_name,
                         typename ProxyMethod<FieldType()>::FieldOnlyConstructorEnabler{}),
                     field_name}
    {
    }

    /// \brief Constructs a ProxyField (only EnableSet is true). Normal ctor that is used in production code.
    template <bool EG = EnableGet, bool ES = EnableSet, typename = std::enable_if_t<!EG && ES>>
    ProxyField(ProxyBase& proxy_base, const std::string_view field_name, detail::EnableSetOnlyTag = {})
        : ProxyField{proxy_base,
                     std::make_unique<ProxyEvent<FieldType>>(
                         proxy_base,
                         ProxyFieldBindingFactory<FieldType>::CreateEventBinding(proxy_base, field_name),
                         field_name,
                         typename ProxyEvent<FieldType>::FieldOnlyConstructorEnabler{}),
                     std::make_unique<ProxyMethod<FieldType(FieldType)>>(
                         proxy_base,
                         ProxyFieldBindingFactory<FieldType>::CreateSetMethodBinding(proxy_base, field_name),
                         field_name,
                         typename ProxyMethod<FieldType(FieldType)>::FieldOnlyConstructorEnabler{}),
                     field_name}
    {
    }

    /// \brief Constructs a ProxyField (both EnableGet and EnableSet are false). Normal ctor that is used in production
    /// code.
    template <bool EG = EnableGet, bool ES = EnableSet, typename = std::enable_if_t<!EG && !ES>>
    ProxyField(ProxyBase& proxy_base, const std::string_view field_name, detail::EnableNeitherTag = {})
        : ProxyField{proxy_base,
                     std::make_unique<ProxyEvent<FieldType>>(
                         proxy_base,
                         ProxyFieldBindingFactory<FieldType>::CreateEventBinding(proxy_base, field_name),
                         field_name,
                         typename ProxyEvent<FieldType>::FieldOnlyConstructorEnabler{}),
                     field_name}
    {
    }

    /// \brief A ProxyField shall not be copyable
    ProxyField(const ProxyField&) = delete;
    ProxyField& operator=(const ProxyField&) = delete;

    /// \brief A ProxyField shall be moveable
    ProxyField(ProxyField&&) noexcept;
    ProxyField& operator=(ProxyField&&) & noexcept;

    ~ProxyField() noexcept = default;

    /**
     * \api
     * \brief Receive pending data from the field.
     * \details The user needs to provide a callable that fulfills the following signature:
     *          void F(SamplePtr<const FieldType>) noexcept. This callback will be called for each sample
     *          that is available at the time of the call. Notice that the number of callback calls cannot
     *          exceed std::min(GetFreeSampleCount(), max_num_samples) times.
     * \tparam F Callable with the signature void(SamplePtr<const FieldType>) noexcept
     * \param receiver Callable with the appropriate signature. GetNewSamples will take ownership
     *                 of this callable.
     * \param max_num_samples Maximum number of samples to return via the given callable.
     * \return Number of samples that were handed over to the callable or an error.
     */
    template <typename F>
    Result<std::size_t> GetNewSamples(F&& receiver, const std::size_t max_num_samples) noexcept
    {
        return proxy_event_dispatch_->GetNewSamples(std::forward<F>(receiver), max_num_samples);
    }

    template <typename T = SampleDataType,
              typename = std::enable_if_t<EnableGet && std::is_same<T, SampleDataType>::value>>
    score::Result<MethodReturnTypePtr<T>> Get() noexcept
    {
        return proxy_method_get_dispatch_->operator()();
    }

    template <typename T = SampleDataType,
              typename = std::enable_if_t<EnableSet && std::is_same<T, SampleDataType>::value>>
    score::Result<MethodReturnTypePtr<T>> Set(SampleDataType& new_field_value) noexcept
    {
        return proxy_method_set_dispatch_->operator()(new_field_value);
    }

    void InjectMock(IProxyEvent<FieldType>& proxy_event_mock)
    {
        proxy_event_dispatch_->InjectMock(proxy_event_mock);
    }

  private:
    /// \brief Private constructor overload for when both EnableGet and EnableSet are true.
    template <bool EG = EnableGet, bool ES = EnableSet, typename = std::enable_if_t<EG && ES>>
    ProxyField(ProxyBase& proxy_base,
               std::unique_ptr<ProxyEvent<FieldType>> proxy_event_dispatch,
               std::unique_ptr<ProxyMethod<FieldType()>> proxy_method_get_dispatch,
               std::unique_ptr<ProxyMethod<FieldType(FieldType)>> proxy_method_set_dispatch,
               const std::string_view field_name)
        : ProxyFieldBase{proxy_base, proxy_event_dispatch.get(), field_name},
          proxy_event_dispatch_{std::move(proxy_event_dispatch)},
          proxy_method_get_dispatch_{std::move(proxy_method_get_dispatch)},
          proxy_method_set_dispatch_{std::move(proxy_method_set_dispatch)}
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(proxy_event_dispatch_ != nullptr);

        ProxyBaseView proxy_base_view{proxy_base};
        proxy_base_view.RegisterField(field_name, *this);
    }

    /// \brief Private constructor overload for when only EnableGet is true.
    template <bool EG = EnableGet, bool ES = EnableSet, typename = std::enable_if_t<EG && !ES>>
    ProxyField(ProxyBase& proxy_base,
               std::unique_ptr<ProxyEvent<FieldType>> proxy_event_dispatch,
               std::unique_ptr<ProxyMethod<FieldType()>> proxy_method_get_dispatch,
               const std::string_view field_name)
        : ProxyFieldBase{proxy_base, proxy_event_dispatch.get(), field_name},
          proxy_event_dispatch_{std::move(proxy_event_dispatch)},
          proxy_method_get_dispatch_{std::move(proxy_method_get_dispatch)}
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(proxy_event_dispatch_ != nullptr);

        ProxyBaseView proxy_base_view{proxy_base};
        proxy_base_view.RegisterField(field_name, *this);
    }

    /// \brief Private constructor overload for when only EnableSet is true.
    template <bool EG = EnableGet, bool ES = EnableSet, typename = std::enable_if_t<!EG && ES>>
    ProxyField(ProxyBase& proxy_base,
               std::unique_ptr<ProxyEvent<FieldType>> proxy_event_dispatch,
               std::unique_ptr<ProxyMethod<FieldType(FieldType)>> proxy_method_set_dispatch,
               const std::string_view field_name)
        : ProxyFieldBase{proxy_base, proxy_event_dispatch.get(), field_name},
          proxy_event_dispatch_{std::move(proxy_event_dispatch)},
          proxy_method_set_dispatch_{std::move(proxy_method_set_dispatch)}
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(proxy_event_dispatch_ != nullptr);

        ProxyBaseView proxy_base_view{proxy_base};
        proxy_base_view.RegisterField(field_name, *this);
    }

    /// \brief Private constructor overload for when both EnableGet and EnableSet are false.
    template <bool EG = EnableGet, bool ES = EnableSet, typename = std::enable_if_t<!EG && !ES>>
    ProxyField(ProxyBase& proxy_base,
               std::unique_ptr<ProxyEvent<FieldType>> proxy_event_dispatch,
               const std::string_view field_name)
        : ProxyFieldBase{proxy_base, proxy_event_dispatch.get(), field_name},
          proxy_event_dispatch_{std::move(proxy_event_dispatch)}
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(proxy_event_dispatch_ != nullptr);

        ProxyBaseView proxy_base_view{proxy_base};
        proxy_base_view.RegisterField(field_name, *this);
    }

    std::unique_ptr<ProxyEvent<FieldType>> proxy_event_dispatch_;

    struct empty
    {
    };

    using ProxyGetMethodType = std::conditional_t<EnableGet, std::unique_ptr<ProxyMethod<FieldType()>>, empty>;
    using ProxySetMethodType = std::conditional_t<EnableSet, std::unique_ptr<ProxyMethod<FieldType(FieldType)>>, empty>;

    ProxyGetMethodType proxy_method_get_dispatch_;
    ProxySetMethodType proxy_method_set_dispatch_;

    static_assert(std::is_same<decltype(proxy_event_dispatch_), std::unique_ptr<ProxyEvent<FieldType>>>::value,
                  "proxy_event_dispatch_ needs to be a unique_ptr since we pass a pointer to it to ProxyFieldBase, so "
                  "we must ensure that it doesn't move when the ProxyField is moved to avoid dangling references. ");
};

template <typename FieldType, const bool EnableGet, const bool EnableSet, const bool EnableNotifier>
ProxyField<FieldType, EnableGet, EnableSet, EnableNotifier>::ProxyField(ProxyField&& other) noexcept
    : ProxyFieldBase(std::move(static_cast<ProxyFieldBase&&>(other))),
      proxy_event_dispatch_(std::move(other.proxy_event_dispatch_)),
      proxy_method_get_dispatch_(std::move(other.proxy_method_get_dispatch_)),
      proxy_method_set_dispatch_(std::move(other.proxy_method_set_dispatch_))
{
    ProxyBaseView proxy_base_view{proxy_base_.get()};
    proxy_base_view.UpdateField(field_name_, *this);
}

template <typename FieldType, const bool EnableGet, const bool EnableSet, const bool EnableNotifier>
// Suppress "AUTOSAR C++14 A6-2-1" rule violation. The rule states "Move and copy assignment operators shall either move
// or respectively copy base classes and data members of a class, without any side effects."
// Rationale: The parent proxy stores a reference to the ProxyEvent. The address that is pointed to must be
// updated when the ProxyField is moved. Therefore, side effects are required.
// coverity[autosar_cpp14_a6_2_1_violation]
auto ProxyField<FieldType, EnableGet, EnableSet, EnableNotifier>::operator=(ProxyField&& other) & noexcept
    -> ProxyField<FieldType, EnableGet, EnableSet, EnableNotifier>&
{
    if (this != &other)
    {
        ProxyFieldBase::operator=(std::move(other));

        ProxyBaseView proxy_base_view{proxy_base_.get()};
        proxy_base_view.UpdateField(field_name_, *this);
        proxy_event_dispatch_ = std::move(other.proxy_event_dispatch_);
        proxy_method_get_dispatch_ = std::move(other.proxy_method_get_dispatch_);
        proxy_method_set_dispatch_ = std::move(other.proxy_method_set_dispatch_);
    }
    return *this;
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PROXY_FIELD_H
