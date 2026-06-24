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

#include "score/mw/com/impl/field_tags.h"
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

template <typename SampleDataType, typename... Tags>
class SkeletonField : public SkeletonFieldBase
{
    // SkeletonField must enable WithGetter or WithNotifier. Without one the consumer has no way to
    // observe the value, so the field is useless. WithSetter alone does not count.
    static_assert(
        contains_type<WithNotifier, Tags...>::value || contains_type<WithGetter, Tags...>::value,
        "A field must either have WithGetter or WithNotifier enabled otherwise, there is no way for the consumer to "
        "receive the field values.");

  public:
    using FieldType = SampleDataType;

    /// \brief Testing ctor that delegates to the production ctor that accepts mock bindings directly, this can only be
    /// used in test code.
    /// \param skeleton_base Parent skeleton that owns this field's registration.
    /// \param field_name Field's name as it appears in the deployment.
    /// \param binding Mock event binding.
    SkeletonField(SkeletonBase& skeleton_base,
                  const std::string_view field_name,
                  std::unique_ptr<SkeletonEventBinding<FieldType>> binding)
        : SkeletonField{skeleton_base,
                        field_name,
                        std::make_unique<SkeletonEvent<FieldType>>(skeleton_base, field_name, std::move(binding)),
                        nullptr,
                        nullptr}
    {
    }

    /// \brief Production ctor used by end users.
    /// \details The MakeGetMethodIfEnabled / MakeSetMethodIfEnabled helpers consult the tag pack at compile time
    ///          and return nullptr when their corresponding tag is absent.
    /// \param parent Parent skeleton that owns this field's registration.
    /// \param field_name Field's name as it appears in the deployment.
    SkeletonField(SkeletonBase& parent, const std::string_view field_name)
        : SkeletonField{parent,
                        field_name,
                        MakeSkeletonEvent(parent, field_name),
                        MakeSetMethodIfEnabled(parent, field_name),
                        MakeGetMethodIfEnabled(parent, field_name)}
    {
        // Each Make*IfEnabled must have produced a non-null dispatch when
        // the corresponding tag is in the pack.
        if constexpr (kHasGetter)
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(get_method_ != nullptr);
        }
        if constexpr (kHasSetter)
        {
            SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(set_method_ != nullptr);
        }
    }

    ~SkeletonField() override = default;

    SkeletonField(const SkeletonField&) = delete;
    SkeletonField& operator=(const SkeletonField&) & = delete;

    SkeletonField(SkeletonField&& other) noexcept = default;
    SkeletonField& operator=(SkeletonField&& other) & noexcept = default;

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
    Result<void> Update(const FieldType& sample_value) noexcept;

    /**
     * \api
     * \brief FieldType is previously allocated by middleware and provided by the user to indicate that he is finished
     *        filling the provided pointer with live data. Dispatches to SkeletonEvent::Send()
     * \param sample The pre-allocated sample pointer containing the field data to be sent.
     * \return On failure, returns an error code.
     */
    Result<void> Update(SampleAllocateePtr<FieldType> sample) noexcept;

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

    /// \brief Registers a handler invoked by the middleware whenever a proxy calls the field setter.
    /// \details Only available when the tag pack includes WithSetter (SFINAE-gated).
    ///
    /// \tparam CallableType Any callable (std::function, score::cpp::callback, lambda, ...) with the signature
    ///         void(FieldType& new_value), where new_value is the proxy-requested value at entry and the
    ///         accepted value at exit.
    /// \param handler User callback to install.
    /// \return void on success otherwise the error code reported by the binding.
    template <typename CallableType,
              typename U = SampleDataType,
              typename = std::enable_if_t<is_tag_enabled<U, SampleDataType, WithSetter, Tags...>::value>>
    Result<void> RegisterSetHandler(CallableType&& set_handler)
    {
        static_assert(std::is_invocable_v<CallableType, FieldType&>,
                      "RegisterSetHandler: handler must be callable as void(FieldType& value). "
                      "The argument initially holds the proxy-requested value and may be modified in-place.");

        auto wrapped_callback =
            [this, set_handler = std::forward<CallableType>(set_handler)](FieldType& new_value) -> FieldType {
            // Allow user to validate/modify the value in-place
            set_handler(new_value);

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
        return set_method_->RegisterHandler(std::move(wrapped_callback));
    }

  private:
    using SetMethodSignature = FieldType(FieldType);
    using GetMethodSignature = FieldType();

    [[nodiscard]] bool IsInitialValueSaved() const noexcept override
    {
        return initial_field_value_ != nullptr;
    }
    Result<void> DoDeferredUpdate() noexcept override;

    /// \brief FieldType is allocated by the user and provided to the middleware to send. Dispatches to
    /// SkeletonEvent::Send()
    Result<void> UpdateImpl(const FieldType& sample_value) noexcept;

    SkeletonEvent<FieldType>* GetTypedEvent() const noexcept;

    [[nodiscard]] bool IsSetHandlerMissing() const noexcept override
    {
        if constexpr (!kHasSetter)
        {
            return false;
        }
        return !is_set_handler_registered_;
    }

    static constexpr bool kHasGetter = contains_type<WithGetter, Tags...>::value;
    static constexpr bool kHasSetter = contains_type<WithSetter, Tags...>::value;

    static std::unique_ptr<SkeletonEvent<FieldType>> MakeSkeletonEvent(SkeletonBase& parent,
                                                                       const std::string_view field_name)
    {
        // No kHasNotifier: the SkeletonEvent is always built because it provides Update/Allocate.
        return std::make_unique<SkeletonEvent<FieldType>>(
            parent,
            field_name,
            SkeletonFieldBindingFactory<SampleDataType>::CreateEventBinding(
                SkeletonBaseView{parent}.GetAssociatedInstanceIdentifier(), parent, field_name),
            typename SkeletonEvent<FieldType>::FieldOnlyConstructorEnabler{});
    }

    /// \brief Builds the Get-method dispatch when WithGetter is enabled.
    /// \return A valid SkeletonMethod dispatch when WithGetter is in the tag pack, nullptr otherwise.
    static std::unique_ptr<SkeletonMethod<GetMethodSignature>> MakeGetMethodIfEnabled(SkeletonBase& parent,
                                                                                      const std::string_view field_name)
    {
        if constexpr (kHasGetter)
        {
            return std::make_unique<SkeletonMethod<GetMethodSignature>>(
                parent,
                field_name,
                ::score::mw::com::impl::MethodType::kGet,
                typename SkeletonMethod<GetMethodSignature>::FieldOnlyConstructorEnabler{});
        }
        else
        {
            return nullptr;
        }
    }

    /// \brief Builds the Set-method dispatch when WithSetter is enabled.
    /// \return A valid SkeletonMethod dispatch when WithSetter is in the tag pack, nullptr otherwise.
    static std::unique_ptr<SkeletonMethod<SetMethodSignature>> MakeSetMethodIfEnabled(SkeletonBase& parent,
                                                                                      const std::string_view field_name)
    {
        if constexpr (kHasSetter)
        {
            return std::make_unique<SkeletonMethod<SetMethodSignature>>(
                parent,
                field_name,
                ::score::mw::com::impl::MethodType::kSet,
                typename SkeletonMethod<SetMethodSignature>::FieldOnlyConstructorEnabler{});
        }
        else
        {
            return nullptr;
        }
    }

    /// \brief Single private delegating constructor. Both the production and test ctors funnel through here with
    ///        appropriate dispatches (real ones from the Make*IfEnabled helpers, or nullptr).
    SkeletonField(SkeletonBase& parent,
                  const std::string_view field_name,
                  std::unique_ptr<SkeletonEvent<FieldType>> skeleton_event_dispatch,
                  std::unique_ptr<SkeletonMethod<SetMethodSignature>> skeleton_set_method_dispatch,
                  std::unique_ptr<SkeletonMethod<GetMethodSignature>> skeleton_get_method_dispatch);

    std::unique_ptr<FieldType> initial_field_value_;
    ISkeletonField<FieldType>* skeleton_field_mock_;

    // Tracks whether RegisterSetHandler() has been called.
    bool is_set_handler_registered_;

    std::unique_ptr<SkeletonMethod<SetMethodSignature>> set_method_;
    std::unique_ptr<SkeletonMethod<GetMethodSignature>> get_method_;
};

template <typename SampleDataType, typename... Tags>
SkeletonField<SampleDataType, Tags...>::SkeletonField(
    SkeletonBase& parent,
    const std::string_view field_name,
    std::unique_ptr<SkeletonEvent<FieldType>> skeleton_event_dispatch,
    std::unique_ptr<SkeletonMethod<SetMethodSignature>> skeleton_set_method_dispatch,
    std::unique_ptr<SkeletonMethod<GetMethodSignature>> skeleton_get_method_dispatch)
    : SkeletonFieldBase{parent, field_name, std::move(skeleton_event_dispatch)},
      initial_field_value_{nullptr},
      skeleton_field_mock_{nullptr},
      is_set_handler_registered_{false},
      set_method_{std::move(skeleton_set_method_dispatch)},
      get_method_{std::move(skeleton_get_method_dispatch)}
{
    SkeletonBaseView skeleton_base_view{parent};
    skeleton_base_view.RegisterField(field_name, GetReferenceToMoveable());
}

/// \brief FieldType is allocated by the user and provided to the middleware to send. Dispatches to
/// SkeletonEvent::Send()
///
/// The initial value of the field must be set before PrepareOffer() is called. However, the actual value of the
/// field cannot be set until the Skeleton has been set up via Skeleton::OfferService(). Therefore, we create a
/// callback that will update the field value with sample_value which will be called in the first call to
/// SkeletonFieldBase::PrepareOffer().
template <typename SampleDataType, typename... Tags>
Result<void> SkeletonField<SampleDataType, Tags...>::Update(const FieldType& sample_value) noexcept
{
    if (skeleton_field_mock_ != nullptr)
    {
        return skeleton_field_mock_->Update(sample_value);
    }

    if (!was_prepare_offer_called_)
    {
        initial_field_value_ = std::make_unique<FieldType>(sample_value);
        return Result<void>{};
    }
    return UpdateImpl(sample_value);
}

/// \brief FieldType is previously allocated by middleware and provided by the user to indicate that he is finished
/// filling the provided pointer with live data. Dispatches to SkeletonEvent::Send()
template <typename SampleDataType, typename... Tags>
Result<void> SkeletonField<SampleDataType, Tags...>::Update(SampleAllocateePtr<FieldType> sample) noexcept
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
template <typename SampleDataType, typename... Tags>
Result<SampleAllocateePtr<SampleDataType>> SkeletonField<SampleDataType, Tags...>::Allocate() noexcept
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

template <typename SampleDataType, typename... Tags>
// Suppress "AUTOSAR C++14 A0-1-3" rule finding. This rule states: "Every function defined in an anonymous
// namespace, or static function with internal linkage, or private member function shall be used.".
// False-positive, method is used in the base class in PrepareOffer().
// coverity[autosar_cpp14_a0_1_3_violation : FALSE]
Result<void> SkeletonField<SampleDataType, Tags...>::DoDeferredUpdate() noexcept
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
    return Result<void>{};
}

template <typename SampleDataType, typename... Tags>
Result<void> SkeletonField<SampleDataType, Tags...>::UpdateImpl(const FieldType& sample_value) noexcept
{
    return GetTypedEvent()->Send(sample_value);
}

template <typename SampleDataType, typename... Tags>
auto SkeletonField<SampleDataType, Tags...>::GetTypedEvent() const noexcept -> SkeletonEvent<SampleDataType>*
{
    auto* const typed_event = dynamic_cast<SkeletonEvent<FieldType>*>(skeleton_event_dispatch_.get());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(typed_event != nullptr, "Downcast to SkeletonEvent<FieldType> failed!");
    return typed_event;
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SKELETON_FIELD_H
