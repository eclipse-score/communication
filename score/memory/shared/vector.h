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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_VECTOR_H
#define BASELIBS_SCORE_MEMORY_SHARED_VECTOR_H

#include "score/memory/shared/polymorphic_offset_ptr_allocator.h"

#include <scoped_allocator>
#include <vector>

namespace score::memory::shared
{

template <class T>
using Vector = std::vector<T, std::scoped_allocator_adaptor<score::memory::shared::PolymorphicOffsetPtrAllocator<T>>>;

template <typename T>
// Suppress "AUTOSAR C++14 A13-5-5", The rule states: "Comparison operators shall be non-member functions with identical
// parameter types and noexcept.".
// Rationale: The justification for the rule is that operators with non-identical operators can be
// confusing. Since there is no functional reason behind and we require comparing a vector using our allocator with a
// vector using a default allocator, we tolerate this deviation.
// coverity[autosar_cpp14_a13_5_5_violation]
inline bool operator==(const Vector<T>& lhs, std::vector<T> rhs) noexcept
{
    return (std::equal(lhs.begin(), lhs.end(), rhs.begin()) == true);
}

template <typename T>
// Suppress "AUTOSAR C++14 A13-5-5", The rule states: "Comparison operators shall be non-member functions with identical
// parameter types and noexcept.".
// Rationale: The justification for the rule is that operators with non-identical operators can be
// confusing. Since there is no functional reason behind and we require comparing a vector using our allocator with a
// vector using a default allocator, we tolerate this deviation.
// coverity[autosar_cpp14_a13_5_5_violation]
inline bool operator==(std::vector<T> lhs, const Vector<T>& rhs) noexcept
{
    return (std::equal(lhs.begin(), lhs.end(), rhs.begin()) == true);
}

}  // namespace score::memory::shared

#endif  // BASELIBS_SCORE_MEMORY_SHARED_VECTOR_H
