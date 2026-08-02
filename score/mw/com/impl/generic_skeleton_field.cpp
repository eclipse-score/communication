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
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/log/logging.h"
#include <cstring>
#include <utility>

namespace score::mw::com::impl
{

GenericSkeletonField::GenericSkeletonField(SkeletonBase& skeleton_base,
                                           const std::string_view field_name,
                                           std::unique_ptr<GenericSkeletonEvent> generic_event,
                                           bool has_getter,
                                           bool has_setter,
                                           bool has_notifier)
    : SkeletonFieldBase{field_name, std::move(generic_event)},
      has_getter_{has_getter},
      has_setter_{has_setter},
      has_notifier_{has_notifier}
{
    // Register this field instance with the parent skeleton's element map
    SkeletonBaseView skeleton_base_view{skeleton_base};
    skeleton_base_view.RegisterField(field_name, GetReferenceToMoveable());
}

Result<void> GenericSkeletonField::Update(SampleAllocateePtr<void> sample) noexcept
{
    return GetGenericEvent()->Send(std::move(sample));
}

Result<void> GenericSkeletonField::Update(score::cpp::span<const uint8_t> raw_value) noexcept
{
    // If OfferService() has not been called yet, we cache the value as the initial value.
    if (!was_prepare_offer_called_)
    {
        initial_field_value_.assign(raw_value.begin(), raw_value.end());
        has_initial_value_ = true;
        return Result<void>{};
    }

    auto alloc_res = Allocate();
    if (!alloc_res.has_value())
    {
        return score::Unexpected{alloc_res.error()};
    }

    auto sample = std::move(alloc_res.value());
    const auto size_info = GetGenericEvent()->GetSizeInfo();

    // Ensure the payload fits in the allocated sample memory
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(raw_value.size() <= size_info.size,
                                                "Raw value payload exceeds configured generic field size.");

    // Copy the raw bytes into the shared memory slot and immediately push to subscribers
    std::memcpy(sample.Get(), raw_value.data(), raw_value.size());
    return Update(std::move(sample));
}

Result<SampleAllocateePtr<void>> GenericSkeletonField::Allocate() noexcept
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

Result<void> GenericSkeletonField::RegisterSetHandler(std::function<void(score::cpp::span<uint8_t>)> /*set_handler*/)
{
    if (!has_setter_)
    {
        return MakeUnexpected(ComErrc::kCouldNotExecute);
    }
    score::mw::log::LogWarn("GenericSkeletonField") << "Setters are currently WIP and cannot be registered.";
    return MakeUnexpected(ComErrc::kCouldNotExecute);
}

bool GenericSkeletonField::IsInitialValueSaved() const noexcept
{
    return has_initial_value_;
}

Result<void> GenericSkeletonField::DoDeferredUpdate() noexcept
{
    SCORE_LANGUAGE_FUTURECPP_ASSERT_MESSAGE(has_initial_value_, "Deferred update requires initial value.");

    auto alloc_res = GetGenericEvent()->Allocate();
    if (!alloc_res.has_value())
    {
        return score::Unexpected{alloc_res.error()};
    }

    auto sample = std::move(alloc_res.value());
    // Transfer the cached initial bytes into the shared memory slot
    std::memcpy(sample.Get(), initial_field_value_.data(), initial_field_value_.size());

    auto res = Update(std::move(sample));

    if (res.has_value())
    {
        has_initial_value_ = false;
        // Clean up cached buffer to free process memory since initial state is now distributed
        initial_field_value_.clear();
    }

    return res;
}

bool GenericSkeletonField::IsSetHandlerMissing() const noexcept
{
    if (has_setter_)
    {
        return !is_set_handler_registered_;
    }
    return false;  // If no setter is required by config, it's not "missing" (OfferService passes)
}

GenericSkeletonEvent* GenericSkeletonField::GetGenericEvent() const noexcept
{
    auto* const typed_event = dynamic_cast<GenericSkeletonEvent*>(skeleton_event_dispatch_.get());
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(typed_event != nullptr, "Downcast to GenericSkeletonEvent failed!");
    return typed_event;
}

}  // namespace score::mw::com::impl
