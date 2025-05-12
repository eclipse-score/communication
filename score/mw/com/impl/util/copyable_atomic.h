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
#ifndef SCORE_MW_COM_IMPL_UTIL_COPYABLE_ATOMIC_H
#define SCORE_MW_COM_IMPL_UTIL_COPYABLE_ATOMIC_H

#include <atomic>

namespace score::mw::com::impl
{

/// \brief Small helper class, which allows "copyable" atomics.
/// \attention The copying itself isn't necessarily atomic!
/// \details A std::atomic<T> is generally NOT copyable for several reasons: Depending on T and the underlying
///          architecture, to achieve "atomicity" eventually a mutex is required for the implementation of the atomic.
///          But mutexes are generally NOT copyable.
///          Best explanation imho: https://stackoverflow.com/a/15250851
/// \tparam T the atomic type. I.e. instead of std::atomic<bool> (which isn't copyable), you can use
/// CopyableAtomic<bool>
template <class T>
class CopyableAtomic
{
  public:
    CopyableAtomic() = default;
    // NOLINTBEGIN(google-explicit-constructor): Explanation provided below.
    // We want to have implicit conversions exactly as std::atomic allows as this is just a wrapper around std::atomic.
    // coverity[autosar_cpp14_a12_1_4_violation]
    explicit CopyableAtomic(T desired) noexcept : atomic_{desired} {}
    // NOLINTEND(google-explicit-constructor): see above for detailed explanation
    CopyableAtomic(const CopyableAtomic& other) noexcept : atomic_{other.atomic_.load()} {}
    // We have to provide a move-ctor because of clan-tidy <cppcoreguidelines-special-member-functions>, since we
    // did explicitly define copy-ctor/copy-assign. Factually we don't want to provide a move-ctor as it shall fall back
    // to copy-ctor. But deleting it would NOT hinder it from being chosen in overload resolution and then lead to an
    // error, if chosen, but deleted. So we declare it as default. Since we have a non-movable member (atomic_), the
    // defaulted move-ctor will be defined as deleted and not take place in overload resolution!
    // \see https://en.cppreference.com/w/cpp/language/move_constructor -> Deleted move constructor
    CopyableAtomic(CopyableAtomic&& other) noexcept = default;
    CopyableAtomic& operator=(const CopyableAtomic& rhs) noexcept
    {
        if (this == &rhs)
        {
            return *this;
        }
        this->atomic_.store(rhs.atomic_.load());
        return *this;
    }

    // Same as move-ctor above!
    CopyableAtomic& operator=(CopyableAtomic&& rhs) noexcept = default;

    ~CopyableAtomic() = default;

    // NOLINTBEGIN(google-explicit-constructor): Explanation provided below.
    // We want to have implicit conversions exactly as std::atomic allows as this is just a wrapper around std::atomic.
    // coverity[autosar_cpp14_a13_5_2_violation]
    operator T() const noexcept
    {
        return atomic_.operator T();
    }
    // NOLINTEND(google-explicit-constructor): see above for detailed explanation
    std::atomic<T>& GetUnderlying()
    {
        // Suppress "AUTOSAR C++14 A9-3-1" rule finding: "Member functions shall not return non-const “raw” pointers or
        // references to private or protected data owned by the class.".
        // CopyableAtomic is just a small wrapper of std::atomic, to make it copyable. So exposing the full
        // functionality of the wrapped member efficiently is intended! API callers get the reference and use it in
        // place without leaving the scope, so the reference remains valid.
        // coverity[autosar_cpp14_a9_3_1_violation]
        return atomic_;
    }

    const std::atomic<T>& GetUnderlying() const
    {
        return atomic_;
    }

  private:
    std::atomic<T> atomic_;
};

}  // namespace score::mw::com::impl

#endif
