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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_METHOD_DATA_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_METHOD_DATA_H

#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/configuration/lola_method_id.h"

#include "score/containers/non_relocatable_vector.h"
#include "score/memory/shared/managed_memory_resource.h"
#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

#include <cstddef>
#include <scoped_allocator>
#include <utility>

namespace score::mw::com::impl::lola
{

class MethodData
{
  public:
    explicit MethodData(const std::size_t number_of_method_call_queue_elements,
                        score::memory::shared::ManagedMemoryResource& memory_resource)
        : method_call_queues_{number_of_method_call_queue_elements, memory_resource}
    {
    }

    // Suppress "AUTOSAR C++14 M11-0-1" rule findings. This rule states: "Member data in non-POD class types shall
    // be private.". There are no class invariants to maintain which could be violated by directly accessing member
    // variables.
    // coverity[autosar_cpp14_m11_0_1_violation]
    containers::NonRelocatableVector<std::pair<LolaMethodId, TypeErasedCallQueue>,
                                     std::scoped_allocator_adaptor<memory::shared::PolymorphicOffsetPtrAllocator<
                                         std::pair<LolaMethodId, TypeErasedCallQueue>>>>
        method_call_queues_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_METHODS_METHOD_DATA_H
