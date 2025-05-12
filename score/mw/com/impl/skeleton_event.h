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
#ifndef SCORE_MW_COM_IMPL_SKELETON_EVENT_H
#define SCORE_MW_COM_IMPL_SKELETON_EVENT_H

#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/sample_allocatee_ptr.h"
#include "score/mw/com/impl/plumbing/skeleton_event_binding_factory.h"
#include "score/mw/com/impl/runtime.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_event_base.h"
#include "score/mw/com/impl/skeleton_event_binding.h"
#include "score/mw/com/impl/tracing/skeleton_event_tracing.h"

#include "score/result/result.h"
#include "score/mw/log/logging.h"

#include <score/string_view.hpp>

#include <memory>
#include <utility>

namespace score::mw::com::impl
{

// False Positive: this is a normal forward declaration.
// coverity[autosar_cpp14_m3_2_3_violation]
template <typename>
class SkeletonField;

template <typename SampleDataType>
class SkeletonEvent : public SkeletonEventBase
{
    template <typename T>
    // Design decission: This friend class provides a view on the internals of SkeletonEvent.
    // This enables us to hide unncecessary internals from the enduser.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SkeletonEventView;

    // SkeletonField uses composition pattern to reuse code from SkeletonEvent. These two classes also have shared
    // private APIs which necessitates the use of the friend keyword.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class SkeletonField<SampleDataType>;

    // Empty struct that is used to make the second constructor only accessible to SkeletonEvent and SkeletonField (as
    // the latter is a friend).
    struct PrivateConstructorEnabler
    {
    };

  public:
    using EventType = SampleDataType;

    /// \brief Constructor that should be called when instantiating a SkeletonEvent within a generated Skeleton. It
    /// should register itself with the skeleton on creation.
    SkeletonEvent(SkeletonBase& skeleton_base, const score::cpp::string_view event_name);

    /// \brief Constructor that should be called by a SkeletonField. This constructor does not register itself with the
    /// skeleton on creation.
    ///
    /// We use PrivateConstructorEnabler as an argument to prevent public usage of this constructor instead of using a
    /// private constructor to allow the constructor to be used with std::make_unique.
    SkeletonEvent(SkeletonBase& skeleton_base,
                  const score::cpp::string_view event_name,
                  std::unique_ptr<SkeletonEventBinding<EventType>> binding,
                  PrivateConstructorEnabler);

    ~SkeletonEvent() override = default;

    SkeletonEvent(const SkeletonEvent&) = delete;
    SkeletonEvent& operator=(const SkeletonEvent&) & = delete;

    SkeletonEvent(SkeletonEvent&& other) noexcept;
    SkeletonEvent& operator=(SkeletonEvent&& other) & noexcept;

    /// \brief EventType is allocated by the user and provided to the middleware to send
    /// \return On failure, returns an error code.
    ResultBlank Send(const EventType& sample_value) noexcept;

    /// \brief EventType is previously allocated by middleware and provided by the user to indicate that he is finished
    /// filling the provided pointer with live data.
    /// \return On failure, returns an error code.
    ResultBlank Send(SampleAllocateePtr<EventType> sample) noexcept;

    /// \brief Allocates memory for EventType for the user to fill it. This is especially necessary for Zero-Copy
    /// implementations.
    Result<SampleAllocateePtr<EventType>> Allocate() noexcept;

  private:
    SkeletonEventBinding<EventType>* GetTypedEventBinding() const noexcept;
};

template <typename SampleDataType>
SkeletonEvent<SampleDataType>::SkeletonEvent(SkeletonBase& skeleton_base, const score::cpp::string_view event_name)
    : SkeletonEventBase{skeleton_base,
                        event_name,
                        SkeletonEventBindingFactory<EventType>::Create(
                            SkeletonBaseView{skeleton_base}.GetAssociatedInstanceIdentifier(),
                            skeleton_base,
                            event_name)}
{
    SkeletonBaseView base_skeleton_view{skeleton_base};
    base_skeleton_view.RegisterEvent(event_name, *this);

    if (binding_ != nullptr)
    {
        const SkeletonBaseView skeleton_base_view{skeleton_base};
        const auto& instance_identifier = skeleton_base_view.GetAssociatedInstanceIdentifier();
        const auto binding_type = binding_->GetBindingType();
        tracing_data_ =
            tracing::GenerateSkeletonTracingStructFromEventConfig(instance_identifier, binding_type, event_name);
        binding_->SetSkeletonEventTracingData(tracing_data_);
    }
}

template <typename SampleDataType>
SkeletonEvent<SampleDataType>::SkeletonEvent(SkeletonBase& skeleton_base,
                                             const score::cpp::string_view event_name,
                                             std::unique_ptr<SkeletonEventBinding<EventType>> binding,
                                             PrivateConstructorEnabler)
    : SkeletonEventBase{skeleton_base, event_name, std::move(binding)}
{
    if (binding_ != nullptr)
    {
        const SkeletonBaseView skeleton_base_view{skeleton_base};
        const auto& instance_identifier = skeleton_base_view.GetAssociatedInstanceIdentifier();
        const auto binding_type = binding_->GetBindingType();
        tracing_data_ =
            tracing::GenerateSkeletonTracingStructFromFieldConfig(instance_identifier, binding_type, event_name);
        binding_->SetSkeletonEventTracingData(tracing_data_);
    }
}

template <typename SampleDataType>
SkeletonEvent<SampleDataType>::SkeletonEvent(SkeletonEvent&& other) noexcept : SkeletonEventBase(std::move(other))
{
    // Since the address of this event has changed, we need update the address stored in the parent skeleton.
    SkeletonBaseView base_skeleton_view{skeleton_base_.get()};
    base_skeleton_view.UpdateEvent(event_name_, *this);
}

template <typename SampleDataType>
auto SkeletonEvent<SampleDataType>::operator=(SkeletonEvent&& other) & noexcept -> SkeletonEvent<SampleDataType>&
{
    if (this != &other)
    {
        SkeletonEventBase::operator=(std::move(other));

        // Since the address of this event has changed, we need update the address stored in the parent skeleton.
        SkeletonBaseView base_skeleton_view{skeleton_base_.get()};
        base_skeleton_view.UpdateEvent(event_name_, *this);
    }
    return *this;
}

template <typename SampleDataType>
ResultBlank SkeletonEvent<SampleDataType>::Send(const EventType& sample_value) noexcept
{
    if (!service_offered_flag_.IsSet())
    {
        score::mw::log::LogError("lola") << "SkeletonEvent::Send with copy failed as Event has not yet been offered";
        return MakeUnexpected(ComErrc::kNotOffered);
    }
    auto tracing_handler = impl::tracing::CreateTracingSendCallback<SampleDataType>(tracing_data_, *binding_);

    const auto send_result = GetTypedEventBinding()->Send(sample_value, std::move(tracing_handler));
    if (!send_result.has_value())
    {
        score::mw::log::LogError("lola") << "SkeletonEvent::Send with copy failed: " << send_result.error().Message()
                                       << ": " << send_result.error().UserMessage();
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    return send_result;
}

template <typename SampleDataType>
ResultBlank SkeletonEvent<SampleDataType>::Send(SampleAllocateePtr<EventType> sample) noexcept
{
    auto tracing_handler =
        impl::tracing::CreateTracingSendWithAllocateCallback<SampleDataType>(tracing_data_, *binding_);

    const auto send_result = GetTypedEventBinding()->Send(std::move(sample), std::move(tracing_handler));
    if (!send_result.has_value())
    {
        score::mw::log::LogError("lola") << "SkeletonEvent::Send zero copy failed: " << send_result.error().Message()
                                       << ": " << send_result.error().UserMessage();
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    return send_result;
}

template <typename SampleDataType>
Result<SampleAllocateePtr<SampleDataType>> SkeletonEvent<SampleDataType>::Allocate() noexcept
{
    if (!service_offered_flag_.IsSet())
    {
        score::mw::log::LogError("lola") << "SkeletonEvent::Allocate failed as Event has not yet been offered";
        return MakeUnexpected(ComErrc::kNotOffered);
    }

    auto allocate_result = GetTypedEventBinding()->Allocate();
    if (!allocate_result.has_value())
    {
        score::mw::log::LogError("lola") << "SkeletonEvent::Allocate failed: " << allocate_result.error().Message()
                                       << ": " << allocate_result.error().UserMessage();
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    return allocate_result;
}

template <typename SampleDataType>
auto SkeletonEvent<SampleDataType>::GetTypedEventBinding() const noexcept -> SkeletonEventBinding<SampleDataType>*
{
    auto* const typed_binding = dynamic_cast<SkeletonEventBinding<EventType>*>(binding_.get());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(typed_binding != nullptr, "Downcast to SkeletonEventBinding<EventType> failed!");
    return typed_binding;
}

template <typename SampleType>
class SkeletonEventView
{
  public:
    explicit SkeletonEventView(SkeletonEvent<SampleType>& skeleton_event) : skeleton_event_{skeleton_event} {}

    SkeletonEventBinding<SampleType>* GetBinding() const noexcept
    {
        return skeleton_event_.GetTypedEventBinding();
    }

  private:
    SkeletonEvent<SampleType>& skeleton_event_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SKELETON_EVENT_H
