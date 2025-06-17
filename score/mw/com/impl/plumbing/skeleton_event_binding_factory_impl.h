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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_SKELETON_EVENT_BINDING_FACTORY_IMPL_H
#define SCORE_MW_COM_IMPL_PLUMBING_SKELETON_EVENT_BINDING_FACTORY_IMPL_H

#include "score/mw/com/impl/bindings/lola/element_fq_id.h"
#include "score/mw/com/impl/bindings/lola/skeleton_event.h"
#include "score/mw/com/impl/instance_identifier.h"
#include "score/mw/com/impl/plumbing/i_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_service_element_binding_factory_impl.h"
#include "score/mw/com/impl/skeleton_base.h"
#include "score/mw/com/impl/skeleton_event_binding.h"

#include <score/assert.hpp>
#include <score/string_view.hpp>

#include <memory>

namespace score::mw::com::impl
{

/// \brief Factory class that dispatches calls to the appropriate binding based on binding information in the
/// deployment configuration.
template <typename SampleType>
class SkeletonEventBindingFactoryImpl : public ISkeletonEventBindingFactory<SampleType>
{
  public:
    std::unique_ptr<SkeletonEventBinding<SampleType>> Create(const InstanceIdentifier& identifier,
                                                             SkeletonBase& parent,
                                                             const score::cpp::string_view event_name) noexcept override;
};

template <typename SampleType>
// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto SkeletonEventBindingFactoryImpl<SampleType>::Create(const InstanceIdentifier& identifier,
                                                         SkeletonBase& parent,
                                                         const score::cpp::string_view event_name) noexcept
    -> std::unique_ptr<SkeletonEventBinding<SampleType>>
{
    return CreateSkeletonServiceElement<SkeletonEventBinding<SampleType>,
                                        lola::SkeletonEvent<SampleType>,
                                        ServiceElementType::EVENT>(identifier, parent, event_name);
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_PLUMBING_SKELETON_EVENT_BINDING_FACTORY_IMPL_H
