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

#include <memory>
#include <type_traits>
#include <utility>

namespace score::mw::com::impl::mock_binding
{

template <typename SampleType>
using SamplePtrCustomDeleter = score::cpp::callback<void(SampleType*)>;

/// \brief SamplePtr used for the mock binding.
///
/// The SamplePtr is an alias for a unique_ptr with a custom deleter. If no deleter is provided, a default deleter
/// will be used. A custom deleter must be supplied when SampleType == void, as calling delete on a void pointer is
/// undefined behaviour (as it's unclear what destructor to call on a void pointer).
///
/// @tparam SampleType The data that is transmitted via the mock proxy.
template <typename SampleType>
using SamplePtr = std::unique_ptr<SampleType, SamplePtrCustomDeleter<SampleType>>;

/// \brief Interim helper rebinding a type-erased SamplePtr<void> into a concrete SamplePtr<SampleType>.
///
/// \details Since SamplePtr is a plain alias for std::unique_ptr, it cannot be given an additional (rebind)
/// constructor itself. This free function fills that role instead: it releases the void pointer from \p other,
/// reinterprets it as SampleType* and wraps the original (void-based) deleter so it can still be invoked with a
/// SampleType* argument.
///
/// \attention This is an INTERIM solution only, needed as long as mock_binding::SamplePtr remains a class template.
/// Once it gets converted into a non-template, fully type-erased class (mirroring what was already done for
/// mock_binding::SampleAllocateePtr), this helper - and the whole rebind mechanism built around it - becomes
/// obsolete and shall be removed.
///
/// \tparam SampleType concrete target type to rebind to; must not be void.
/// \param other type-erased SamplePtr<void> to rebind. Left empty (owning nothing) afterwards.
/// \return a SamplePtr<SampleType> now owning what \p other used to own.
///
/// \note The original (void-based) deleter is heap-allocated behind a shared_ptr rather than captured by value in
/// the new deleter lambda: SamplePtrCustomDeleter itself already uses (almost) the whole small-buffer-optimized
/// capacity of score::cpp::callback, so capturing it by value would exceed the wrapping callback's capacity.
template <typename SampleType, typename = std::enable_if_t<!std::is_void<SampleType>::value>>
SamplePtr<SampleType> RebindSamplePtr(SamplePtr<void>&& other) noexcept
{
    auto deleter = std::make_shared<SamplePtrCustomDeleter<void>>(std::move(other.get_deleter()));
    auto* const raw_pointer = static_cast<SampleType*>(other.release());
    return SamplePtr<SampleType>{raw_pointer, [deleter](SampleType* pointer) mutable noexcept {
                                     (*deleter)(static_cast<void*>(pointer));
                                 }};
}

}  // namespace score::mw::com::impl::mock_binding

#endif  // SCORE_MW_COM_IMPL_BINDINGS_MOCK_BINDING_SAMPLE_PTR_H
