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
#include "score/mw/com/impl/plumbing/skeleton_field_binding_factory_impl.h"

#include "score/mw/com/impl/plumbing/skeleton_service_element_binding_factory_impl.h"

namespace score::mw::com::impl
{
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto SkeletonFieldBindingFactoryImpl::CreateEventBinding(const InstanceIdentifier& identifier,
                                                         SkeletonBinding& parent_binding,
                                                         const std::string_view field_name,
                                                         memory::DataTypeSizeInfo sample_type_size_info,
                                                         const FieldTagsStore field_tags_store) noexcept
    -> std::unique_ptr<SkeletonEventBinding>
{
    return CreateSkeletonEventOrField<SkeletonEventBinding, lola::SkeletonEvent, ServiceElementType::FIELD>(
        identifier, parent_binding, field_name, sample_type_size_info, field_tags_store);
}
}  // namespace score::mw::com::impl
