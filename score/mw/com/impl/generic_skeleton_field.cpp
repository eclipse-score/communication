/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/
#include "score/mw/com/impl/generic_skeleton_field.h"
#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/plumbing/skeleton_event_binding_factory.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/log/logging.h"
#include <cstring>
#include <utility>

namespace score::mw::com::impl
{

std::unique_ptr<GenericSkeletonEvent> GenericSkeletonField::MakeGenericEvent(
    SkeletonBase& skeleton_base,
    const std::string_view field_name,
    const score::memory::DataTypeSizeInfo size_info)
{
    const SkeletonBaseView skeleton_base_view{skeleton_base};
    auto event_binding_ptr = SkeletonEventBindingFactory::Create(
        skeleton_base_view.GetAssociatedInstanceIdentifier(), skeleton_base_view.GetBinding(), field_name, size_info);

    return std::make_unique<GenericSkeletonEvent>(
        skeleton_base, field_name, std::move(event_binding_ptr), GenericSkeletonEvent::FieldOnlyConstructorEnabler{});
}

GenericSkeletonField::GenericSkeletonField(SkeletonBase& skeleton_base,
                                           const std::string_view field_name,
                                           const score::memory::DataTypeSizeInfo size_info,
                                           bool has_getter,
                                           bool has_setter)
    : SkeletonFieldBase{field_name, MakeGenericEvent(skeleton_base, field_name, size_info)},
      initial_field_value_{size_info.Size()},
      has_initial_value_{false},
      has_getter_{has_getter},
      has_setter_{has_setter},
      is_set_handler_registered_{false}
{
    // Register this field instance with the parent skeleton's element map
    SkeletonBaseView skeleton_base_view{skeleton_base};
    skeleton_base_view.RegisterField(field_name, GetReferenceToMoveable());
}

GenericSkeletonField::GenericSkeletonField(SkeletonBase& skeleton_base,
                                           const std::string_view field_name,
                                           const score::memory::DataTypeSizeInfo size_info,
                                           std::unique_ptr<GenericSkeletonEvent> generic_event,
                                           bool has_getter,
                                           bool has_setter)
    : SkeletonFieldBase{field_name, std::move(generic_event)},
      initial_field_value_{size_info.Size()},
      has_initial_value_{false},
      has_getter_{has_getter},
      has_setter_{has_setter},
      is_set_handler_registered_{false}
{
    // Register this field instance with the parent skeleton's element map
    SkeletonBaseView skeleton_base_view{skeleton_base};
    skeleton_base_view.RegisterField(field_name, GetReferenceToMoveable());
}

Result<void> GenericSkeletonField::Update(SampleAllocateePtr<void> sample)
{
    return GetGenericEvent()->Send(std::move(sample));
}

Result<void> GenericSkeletonField::Update(score::cpp::span<const uint8_t> raw_value)
{
    // If OfferService() has not been called yet, we cache the value as the initial value.
    if (!was_prepare_offer_called_)
    {
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(raw_value.size() <= initial_field_value_.capacity(),
                                                    "Raw value payload exceeds configured generic field capacity.");
        if (has_initial_value_)
        {
            initial_field_value_ = score::containers::NonRelocatableVector<uint8_t>{initial_field_value_.capacity()};
        }
        for (const auto byte : raw_value)
        {
            initial_field_value_.emplace_back(byte);
        }
        has_initial_value_ = true;
        return Result<void>{};
    }

    return UpdateImpl(raw_value);
}

Result<void> GenericSkeletonField::UpdateImpl(score::cpp::span<const uint8_t> raw_value)
{
    return GetGenericEvent()->Send(raw_value);
}

Result<SampleAllocateePtr<void>> GenericSkeletonField::Allocate()
{
    if (!was_prepare_offer_called_)
    {
        // Shared memory backing the sample allocator isn't active until OfferService is called.
        score::mw::log::LogWarn("GenericSkeletonField")
            << "Cannot allocate memory for Generic Field before OfferService() is called.";
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    return GetGenericEvent()->Allocate();
}

bool GenericSkeletonField::IsInitialValueSaved() const noexcept
{
    return has_initial_value_;
}

Result<void> GenericSkeletonField::DoDeferredUpdate() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(has_initial_value_, "Deferred update requires initial value.");

    score::cpp::span<const uint8_t> initial_value_span{initial_field_value_.data(), initial_field_value_.size()};
    const auto update_result = UpdateImpl(initial_value_span);
    if (!update_result.has_value())
    {
        return update_result;
    }

    has_initial_value_ = false;
    return Result<void>{};
}

bool GenericSkeletonField::IsSetHandlerMissing() const noexcept
{
    if (has_setter_)
    {
        return !is_set_handler_registered_;
    }
    return false;  // If no setter is required by config, it's not "missing" (OfferService passes)
}

bool GenericSkeletonField::HasValidBinding() const noexcept
{
    return (skeleton_event_dispatch_ != nullptr) &&
           (SkeletonFieldBaseView{*const_cast<GenericSkeletonField*>(this)}.GetEventBinding() != nullptr);
}

Result<void> GenericSkeletonField::RegisterGetHandler()
{
    // If this field is not configured with a Getter, we don't need to do any wiring.
    if (!has_getter_)
    {
        return {};
    }

    // TODO: Implement generic Getter wiring once GenericSkeletonMethod is available.
    // For now, return an error because generic getters are WIP.
    return MakeUnexpected(ComErrc::kCouldNotExecute);
}

GenericSkeletonEvent* GenericSkeletonField::GetGenericEvent() const
{
    auto* const generic_event = dynamic_cast<GenericSkeletonEvent*>(skeleton_event_dispatch_.get());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(generic_event != nullptr, "Downcast to GenericSkeletonEvent failed!");
    return generic_event;
}

}  // namespace score::mw::com::impl
