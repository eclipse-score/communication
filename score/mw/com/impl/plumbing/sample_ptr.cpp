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
#include "score/mw/com/impl/plumbing/sample_ptr.h"

#include "score/mw/com/impl/bindings/lola/sample_allocatee_ptr.h"
#include "score/mw/com/impl/bindings/mock_binding/sample_allocatee_ptr.h"

#include <score/overload.hpp>

#include <exception>
#include <utility>

namespace score::mw::com::impl
{

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
SamplePtr<void> CreateSamplePtrFromSampleAllocateePtr(SampleAllocateePtr<void>& sample_allocatee_ptr)
{
    auto& binding_ptr_variant = SampleAllocateePtrMutableView{sample_allocatee_ptr}.GetUnderlyingVariant();
    auto visitor = score::cpp::overload(
        [](lola::SampleAllocateePtr& lola_ptr) -> SamplePtr<void> {
            lola::ConsumerEventDataControlLocalView<>& consumer_event_data_control_local =
                lola::SampleAllocateePtrMutableView{lola_ptr}.GetConsumerEventDataControlLocalView();

            const auto event_slot_index = lola_ptr.GetReferencedSlot();
            consumer_event_data_control_local.ReferenceSpecificEvent(event_slot_index);
            const auto* const managed_object =
                static_cast<const void*>(lola::SampleAllocateePtrView{lola_ptr}.GetManagedObject());

            lola::SamplePtr lola_sample_ptr{managed_object, consumer_event_data_control_local, event_slot_index};
            return SamplePtr<void>{std::move(lola_sample_ptr), SampleReferenceGuard{}};
        },
        // Tracing is not supported on SomeIP.
        [](someip::SampleAllocateePtr&) -> SamplePtr<void> {
            std::terminate();
        },
        // Suppress "AUTOSAR C++14 A8-4-12" rule finding. This rule states: "A std::unique_ptr shall be passed to a
        // function as: (1) a copy to express the function assumes ownership (2) an lvalue reference to express that
        // the function replaces the managed object".
        // Here we can't use a raw pointer / reference since we're using score::cpp::overload, and the function is
        // not replacing the managed object, so this should be a non-const reference (matching the lola case above).
        // coverity[autosar_cpp14_a8_4_12_violation]
        [](mock_binding::SampleAllocateePtr& ptr) -> SamplePtr<void> {
            return SamplePtr<void>{mock_binding::SamplePtr{ptr.get(), [](void*) noexcept {}}, SampleReferenceGuard{}};
        },
        // LCOV_EXCL_START (Defensive programming: CreateSamplePtrFromSampleAllocateePtr is always expected to be
        // called with a valid, non-blank SampleAllocateePtr. Callers are expected to have already verified this,
        // e.g. via ExtractBindingTracingData, which terminates on a blank SampleAllocateePtr. Therefore, we will
        // never reach this branch.
        [](score::cpp::blank&) -> SamplePtr<void> {
            std::terminate();
        });
    // LCOV_EXCL_STOP

    return std::visit(visitor, binding_ptr_variant);
}

}  // namespace score::mw::com::impl
