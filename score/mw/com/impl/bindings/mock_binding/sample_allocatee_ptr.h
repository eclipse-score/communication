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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SAMPLE_ALLOCATEE_PTR_H
#define SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SAMPLE_ALLOCATEE_PTR_H

#include <score/callback.hpp>
#include <memory>

namespace score::mw::com::impl::mock_binding
{

template <typename SampleType>
using CustomDeleter = score::cpp::callback<void(SampleType*)>;

/// \brief SampleAllocateePtr used for the mock binding.
///
/// The SampleAllocateePtr is an alias for a unique_ptr with a custom deleter. 
/// This matches the logic used in SamplePtr to support 'void' types safely.
///
/// @tparam SampleType The data type managed by this pointer.
template <typename SampleType>
using SampleAllocateePtr = std::unique_ptr<SampleType, CustomDeleter<SampleType>>;

}  // namespace score::mw::com::impl::mock_binding

#endif  // SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SAMPLE_ALLOCATEE_PTR_H