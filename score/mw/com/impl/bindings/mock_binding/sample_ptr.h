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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SAMPLE_PTR_H
#define SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SAMPLE_PTR_H

#include <score/callback.hpp>

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace score::mw::com::impl::mock_binding
{

using SamplePtrCustomDeleter = score::cpp::callback<void(void*)>;

/// \brief SamplePtr used for the mock binding.
///
/// The SamplePtr is an alias for a unique_ptr with a custom deleter. If no deleter is provided, a default deleter
/// will be used. A custom deleter must be supplied when SampleType == void, as calling delete on a void pointer is
/// undefined behaviour (as it's unclear what destructor to call on a void pointer).
using SamplePtr = std::unique_ptr<void, SamplePtrCustomDeleter>;

template <typename SampleType>
SamplePtr MakeSamplePtr(SampleType* sample_ptr)
{
    return SamplePtr{sample_ptr, [](void* p) noexcept {
                         delete static_cast<SampleType*>(p);
                     }};
}

}  // namespace score::mw::com::impl::mock_binding

#endif  // SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SAMPLE_PTR_H
