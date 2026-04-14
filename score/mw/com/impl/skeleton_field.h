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
#ifndef SCORE_MW_COM_IMPL_SKELETON_FIELD_H
#define SCORE_MW_COM_IMPL_SKELETON_FIELD_H

#include "score/mw/com/impl/method_type.h"
#include "score/mw/com/impl/methods/skeleton_method.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/plumbing/skeleton_field_binding_factory.h"
#include "score/mw/com/impl/skeleton_event.h"
#include "score/mw/com/impl/skeleton_field_base.h"

#include "score/mw/com/impl/mocking/i_skeleton_field.h"

#include "score/mw/log/logging.h"
#include "score/result/result.h"

#include <score/assert.hpp>

#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace score::mw::com::impl
{

template <typename SampleDataType, const bool EnableSet = false, const bool EnableNotifier = false>
class SkeletonField : public SkeletonFieldBase
{
  public:
    using FieldType = SampleDataType;

    /// \brief Constructs a SkeletonField with setter enabled. Normal ctor used in production code.
    ///
    /// \param parent Skeleton that contains this field.
    /// \param field_name Field name of the field.
    /// \param detail::EnableSetOnlyTag This parameter is only used for constructor overload disambiguation and
    /// has no semantic meaning. The tag disambiguates the setter-enabled ctor from the no-setter ctor, as
    /// otherwise both would have the same signature.
    template <bool ES = EnableSet, typename = std::enable_if_t<ES>>
    SkeletonField(SkeletonBase& parent, const std::string_view field_name, detail::EnableSetOnlyTag = {});

    /// \brief Constructs a SkeletonField with no setter. Normal ctor used in production code.
    ///
    /// \param parent Skeleton that contains this field.
    /// \param field_name Field name of the field.
    /// \param detail::EnableNeitherTag This parameter is only used for constructor overload disambiguation and
    /// has no semantic meaning.
    template <bool ES = EnableSet, typename = std::enable_if_t<!ES>>
    SkeletonField(SkeletonBase& parent, const std::string_view field_name, detail::EnableNeitherTag = {});

    /// Constructor that allows to set the binding directly.
    ///
    /// This is used only used for testing.
    SkeletonField(SkeletonBase& skeleton_base,
                  const std::string_view field_name,
                  std::unique_ptr<SkeletonEventBinding<FieldType>> binding);

    ~SkeletonField() override = default;

    SkeletonField(const SkeletonField&) = delete;
    SkeletonField& operator=(const SkeletonField&) & = delete;

    SkeletonField(SkeletonField&& other) noexcept;
    SkeletonField& operator=(SkeletonField&& other) & noexcept;

    /**
     * \api
     * \brief FieldType is allocated by the user and provided to the middleware to send. Dispatches to
     *        SkeletonEvent::Send()
     * \details The initial value of the field must be set before PrepareOffer() is called. However, the actual value of
     *          the field cannot be set until the Skeleton has been set up via Skeleton::OfferService(). Therefore, we
     *          create a callback that will update the field value with sample_value which will be called in the first
     *          call to SkeletonFieldBase::PrepareOffer().
     * \param sample_value The field data to be sent to subscribers.
     * \return On failure, returns an error code.
     */
    ResultBlank Update(const FieldType& sample_value) noexcept;

    /**
     * \api
     * \brief FieldType is previously allocated by middleware and provided by the user to indicate that he is finished
     *        filling the provided pointer with live data. Dispatches to SkeletonEvent::Send()
     * \param sample The pre-allocated sample pointer containing the field data to be sent.
     * \return On failure, returns an error code.
     */
    ResultBlank Update(SampleAllocateePtr<FieldType> sample) noexcept;

    /**
     * \api
     * \brief Allocates memory for FieldType for the user to fill it. This is especially necessary for Zero-Copy
     *        implementations. Dispatches to SkeletonEvent::Allocate()
     * \details This function cannot be currently called to set the initial value of a field as the shared memory must
     *          be first set up in the Skeleton::PrepareOffer() before the user can obtain / use a SampleAllocateePtr.
     * \return On success, returns a SampleAllocateePtr that can be filled with data. On failure, returns an error code.
     */
    Result<SampleAllocateePtr<FieldType>> Allocate() noexcept;

    void InjectMock(ISkeletonField<FieldType>& skeleton_field_mock)
    {
        skeleton_field_mock_ = &skeleton_field_mock;
    }

    // SFINAE-gated: only exists when EnableSet == true
    //
    // \ brief Registers a handler that is invoked by the middleware whenever a proxy calls the field setter.
    //
    // \tparam CallableType Any callable (std::function, score::cpp::callback, lambda, ...) with the signature:
    //         void(FieldType& new_value)
    //   - new_value : the value requested by the proxy.
    template <bool ES = EnableSet, typename std::enable_if<ES, int>::type = 0, typename CallableType>
    ResultBlank RegisterSetHandler(CallableType&& handler)
    {
        static_assert(std::is_invocable_v<CallableType, FieldType&>,
                      "RegisterSetHandler: handler must be callable as void(FieldType& value). "
                      "The argument initially holds the proxy-requested value and may be modified in-place.");
        set_handler_ = std::move(handler);

        auto wrapped_callback = [this](FieldType& new_value) -> FieldType {
            // Allow user to validate/modify the value in-place
            set_handler_(new_value);

            // Store the (possibly modified) value as the latest field value
            auto update_result = this->Update(new_value);
            if (!update_result.has_value())
            {
                score::mw::log::LogError("lola") << "Set handler: failed to update field value.";
            }

            // Return the accepted value to the proxy
            return new_value;
        };

        is_set_handler_registered_ = true;
        return set_method_.get()->RegisterHandler(std::move(wrapped_callback));
    }

  private:
    bool IsInitialValueSaved() const noexcept override
    {
        return initial_field_value_ != nullptr;
    }
    ResultBlank DoDeferredUpdate() noexcept override;

    /// \brief FieldType is allocated by the user and provided to the middleware to send. Dispatches to
    /// SkeletonEvent::Send()
    ResultBlank UpdateImpl(const FieldType& sample_value) noexcept;

    SkeletonEvent<FieldType>* GetTypedEvent() const noexcept;

    std::unique_ptr<FieldType> initial_field_value_;
    ISkeletonField<FieldType>* skeleton_field_mock_;

    // Zero-cost conditional storage: unique_ptr when EnableSet=true, zero-size tag when false.
    using SetMethodSignature = FieldType(FieldType);
    using SetMethodType =
        std::conditional_t<EnableSet, std::unique_ptr<SkeletonMethod<SetMethodSignature>>, detail::EnableSetOnlyTag>;
    SetMethodType set_method_;

    // Stores the user-provided set handler. Kept as a member so that the wrapped
    // callback can invoke it via this->set_handler_. The concrete storage type is
    // score::cpp::callback with the expected signature so that any callable provided
    // to RegisterSetHandler is type-erased here.
    // Zero-cost when EnableSet=false.
    using SetHandlerStorageType =
        std::conditional_t<EnableSet, score::cpp::callback<void(FieldType&)>, detail::EnableSetOnlyTag>;
    SetHandlerStorageType set_handler_{};

    // Tracks whether RegisterSetHandler() has been called. Zero-cost when EnableSet=false.
    using IsSetHandlerRegisteredType = std::conditional_t<EnableSet, bool, detail::EnableSetOnlyTag>;
    IsSetHandlerRegisteredType is_set_handler_registered_{};

    // EnableSet=true: checks the flag; EnableSet=false: no setter, no handler required.
    bool IsSetHandlerRegistered() const noexcept override
    {
        if constexpr (EnableSet)
        {
            return is_set_handler_registered_;
        }
        return true;
    }

    /// \brief Private delegating constructor used by the setter-enabled public ctor.
    template <bool ES = EnableSet, typename = std::enable_if_t<ES>>
    SkeletonField(SkeletonBase& parent,
                  std::unique_ptr<SkeletonEvent<FieldType>> skeleton_event_dispatch,
                  const std::string_view field_name,
                  detail::EnableSetOnlyTag);

    /// \brief Private delegating constructor used by the no-setter public ctor and testing ctor.
    SkeletonField(SkeletonBase& parent,
                  std::unique_ptr<SkeletonEvent<FieldType>> skeleton_event_dispatch,
                  const std::string_view field_name);

    // TODO: Move get_method_ initialization into the delegating constructors (like set_method_) once the
    // Get handler is implemented.
    using GetMethodSignature = FieldType();
    std::unique_ptr<SkeletonMethod<GetMethodSignature>> get_method_{
        std::make_unique<SkeletonMethod<GetMethodSignature>>(
            skeleton_base_.get(),
            field_name_,
            ::score::mw::com::impl::MethodType::kGet,
            typename SkeletonMethod<GetMethodSignature>::FieldOnlyConstructorEnabler{})};
};

/// \brief Public ctor — EnableSet=true: delegates to the private ctor that also creates the set method.
template <typename SampleDataType, bool EnableSet, bool EnableNotifier>
template <bool ES, typename>
SkeletonField<SampleDataType, EnableSet, EnableNotifier>::SkeletonField(SkeletonBase& parent,
                                                                        const std::string_view field_name,
                                                                        detail::EnableSetOnlyTag)
    : SkeletonField{
          parent,
          std::make_unique<SkeletonEvent<FieldType>>(parent,
                                                     field_name,
                                                     SkeletonFieldBindingFactory<SampleDataType>::CreateEventBinding(
                                                         SkeletonBaseView{parent}.GetAssociatedInstanceIdentifier(),
                                                         parent,
                                                         field_name),
                                                     typename SkeletonEvent<FieldType>::FieldOnlyConstructorEnabler{}),
          field_name,
          detail::EnableSetOnlyTag{}}
{
}

/// \brief Public ctor — EnableSet=false: delegates to the private ctor with no set method.
template <typename SampleDataType, bool EnableSet, bool EnableNotifier>
template <bool ES, typename>
SkeletonField<SampleDataType, EnableSet, EnableNotifier>::SkeletonField(SkeletonBase& parent,
                                                                        const std::string_view field_name,
                                                                        detail::EnableNeitherTag)
    : SkeletonField{
          parent,
          std::make_unique<SkeletonEvent<FieldType>>(parent,
                                                     field_name,
                                                     SkeletonFieldBindingFactory<SampleDataType>::CreateEventBinding(
                                                         SkeletonBaseView{parent}.GetAssociatedInstanceIdentifier(),
                                                         parent,
                                                         field_name),
                                                     typename SkeletonEvent<FieldType>::FieldOnlyConstructorEnabler{}),
          field_name}
{
}

/// \brief Testing ctor: binding is provided directly (used with mock bindings in tests).
template <typename SampleDataType, bool EnableSet, bool EnableNotifier>
SkeletonField<SampleDataType, EnableSet, EnableNotifier>::SkeletonField(
    SkeletonBase& skeleton_base,
    const std::string_view field_name,
    std::unique_ptr<SkeletonEventBinding<FieldType>> binding)
    : SkeletonField{skeleton_base,
                    std::make_unique<SkeletonEvent<FieldType>>(skeleton_base, field_name, std::move(binding)),
                    field_name}
{
}

/// \brief Private delegating ctor — setter enabled.
template <typename SampleDataType, bool EnableSet, bool EnableNotifier>
template <bool ES, typename>
SkeletonField<SampleDataType, EnableSet, EnableNotifier>::SkeletonField(
    SkeletonBase& parent,
    std::unique_ptr<SkeletonEvent<FieldType>> skeleton_event_dispatch,
    const std::string_view field_name,
    detail::EnableSetOnlyTag)
    : SkeletonFieldBase{parent, field_name, std::move(skeleton_event_dispatch)},
      initial_field_value_{nullptr},
      skeleton_field_mock_{nullptr}
{
    set_method_ = std::make_unique<SkeletonMethod<SetMethodSignature>>(
        parent,
        field_name_,
        ::score::mw::com::impl::MethodType::kSet,
        typename SkeletonMethod<SetMethodSignature>::FieldOnlyConstructorEnabler{});
    SkeletonBaseView skeleton_base_view{parent};
    skeleton_base_view.RegisterField(field_name, *this);
}

/// \brief Private delegating ctor — no setter. Receives the already-constructed event.
template <typename SampleDataType, bool EnableSet, bool EnableNotifier>
SkeletonField<SampleDataType, EnableSet, EnableNotifier>::SkeletonField(
    SkeletonBase& parent,
    std::unique_ptr<SkeletonEvent<FieldType>> skeleton_event_dispatch,
    const std::string_view field_name)
    : SkeletonFieldBase{parent, field_name, std::move(skeleton_event_dispatch)},
      initial_field_value_{nullptr},
      skeleton_field_mock_{nullptr}
{
    SkeletonBaseView skeleton_base_view{parent};
    skeleton_base_view.RegisterField(field_name, *this);
}

template <typename SampleDataType, bool EnableSet, bool EnableNotifier>
SkeletonField<SampleDataType, EnableSet, EnableNotifier>::SkeletonField(SkeletonField&& other) noexcept
    : SkeletonFieldBase(static_cast<SkeletonFieldBase&&>(other)),
      // known llvm bug (https://github.com/llvm/llvm-project/issues/63202)
      // This usage is safe because the previous line only moves the base class portion via static_cast.
      // The derived class member 'initial_field_value_' remains untouched by the base class move constructor,
      // so it's still valid to access it here for moving into our own member.
      // coverity[autosar_cpp14_a12_8_3_violation] This is a false-positive.
      initial_field_value_{std::move(other.initial_field_value_)},
      skeleton_field_mock_{other.skeleton_field_mock_},
      set_method_{std::move(other.set_method_)},
      set_handler_{std::move(other.set_handler_)},
      is_set_handler_registered_{std::move(other.is_set_handler_registered_)},
      get_method_{std::move(other.get_method_)}
{
    SkeletonBaseView skeleton_base_view{skeleton_base_.get()};
    skeleton_base_view.UpdateField(field_name_, *this);
}

template <typename SampleDataType, bool EnableSet, bool EnableNotifier>
auto SkeletonField<SampleDataType, EnableSet, EnableNotifier>::operator=(SkeletonField&& other) & noexcept
    -> SkeletonField<SampleDataType, EnableSet, EnableNotifier>&
{
    if (this != &other)
    {
        SkeletonFieldBase::operator=(std::move(other));

        initial_field_value_ = std::move(other.initial_field_value_);
        skeleton_field_mock_ = std::move(other.skeleton_field_mock_);
        set_method_ = std::move(other.set_method_);
        set_handler_ = std::move(other.set_handler_);
        is_set_handler_registered_ = std::move(other.is_set_handler_registered_);
        get_method_ = std::move(other.get_method_);
        SkeletonBaseView skeleton_base_view{skeleton_base_.get()};
        skeleton_base_view.UpdateField(field_name_, *this);
    }
    return *this;
}

/// \brief FieldType is allocated by the user and provided to the middleware to send. Dispatches to
/// SkeletonEvent::Send()
///
/// The initial value of the field must be set before PrepareOffer() is called. However, the actual value of the
/// field cannot be set until the Skeleton has been set up via Skeleton::OfferService(). Therefore, we create a
/// callback that will update the field value with sample_value which will be called in the first call to
/// SkeletonFieldBase::PrepareOffer().
template <typename SampleDataType, bool EnableSet, bool EnableNotifier>
ResultBlank SkeletonField<SampleDataType, EnableSet, EnableNotifier>::Update(const FieldType& sample_value) noexcept
{
    if (skeleton_field_mock_ != nullptr)
    {
        return skeleton_field_mock_->Update(sample_value);
    }

    if (!was_prepare_offer_called_)
    {
        initial_field_value_ = std::make_unique<FieldType>(sample_value);
        return ResultBlank{};
    }
    return UpdateImpl(sample_value);
}

/// \brief FieldType is previously allocated by middleware and provided by the user to indicate that he is finished
/// filling the provided pointer with live data. Dispatches to SkeletonEvent::Send()
template <typename SampleDataType, bool EnableSet, bool EnableNotifier>
ResultBlank SkeletonField<SampleDataType, EnableSet, EnableNotifier>::Update(
    SampleAllocateePtr<FieldType> sample) noexcept
{
    if (skeleton_field_mock_ != nullptr)
    {
        return skeleton_field_mock_->Update(std::move(sample));
    }

    return GetTypedEvent()->Send(std::move(sample));
}

/// \brief Allocates memory for FieldType for the user to fill it. This is especially necessary for Zero-Copy
/// implementations. Dispatches to SkeletonEvent::Allocate()
///
/// This function cannot be currently called to set the initial value of a field as the shared memory must be first
/// set up in the Skeleton::PrepareOffer() before the user can obtain / use a SampleAllocateePtr.
template <typename SampleDataType, bool EnableSet, bool EnableNotifier>
Result<SampleAllocateePtr<SampleDataType>> SkeletonField<SampleDataType, EnableSet, EnableNotifier>::Allocate() noexcept
{
    if (skeleton_field_mock_ != nullptr)
    {
        return skeleton_field_mock_->Allocate();
    }

    // This check can be removed when Ticket-104261 is implemented
    if (!was_prepare_offer_called_)
    {
        score::mw::log::LogWarn("lola")
            << "Lola currently doesn't support zero-copy Allocate() before OfferService() is "
               "called as the shared memory is not setup until OfferService() is called.";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    return GetTypedEvent()->Allocate();
}

template <typename SampleDataType, bool EnableSet, bool EnableNotifier>
// Suppress "AUTOSAR C++14 A0-1-3" rule finding. This rule states: "Every function defined in an anonymous
// namespace, or static function with internal linkage, or private member function shall be used.".
// False-positive, method is used in the base class in PrepareOffer().
// coverity[autosar_cpp14_a0_1_3_violation : FALSE]
ResultBlank SkeletonField<SampleDataType, EnableSet, EnableNotifier>::DoDeferredUpdate() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(
        initial_field_value_ != nullptr,
        "Initial field value containing a value is a precondition for DoDeferredUpdate.");
    const auto update_result = UpdateImpl(*initial_field_value_);
    if (!update_result.has_value())
    {
        return update_result;
    }

    // If the Update call succeeded, then we can delete the initial value
    initial_field_value_.reset();
    return ResultBlank{};
}

template <typename SampleDataType, bool EnableSet, bool EnableNotifier>
ResultBlank SkeletonField<SampleDataType, EnableSet, EnableNotifier>::UpdateImpl(const FieldType& sample_value) noexcept
{
    return GetTypedEvent()->Send(sample_value);
}

template <typename SampleDataType, bool EnableSet, bool EnableNotifier>
auto SkeletonField<SampleDataType, EnableSet, EnableNotifier>::GetTypedEvent() const noexcept
    -> SkeletonEvent<SampleDataType>*
{
    auto* const typed_event = dynamic_cast<SkeletonEvent<FieldType>*>(skeleton_event_dispatch_.get());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(typed_event != nullptr, "Downcast to SkeletonEvent<FieldType> failed!");
    return typed_event;
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SKELETON_FIELD_H
