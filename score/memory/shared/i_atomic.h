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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_MOCK_IATOMIC_H
#define BASELIBS_SCORE_MEMORY_SHARED_MOCK_IATOMIC_H

#include <atomic>

namespace score::memory::shared
{

/// \brief Interface abstraction for a subset of methods from std::atomic, needed/used for mocking support
/// \details The subset of methods within this interface are those methods, which are used by us AND their outcome
///          influences, which code paths are taken. So for coverage reasons, we have to be able to mock them. For a
///          documentation about its use see class AtomicIndirectorReal!
///          Right now, we haven't added further methods like:
///          - std::atomic<T>::operator=
///          to this interface, although we are using them in code. But since we currently have not yet a need to be
///          able, to mock them in our test cases, those are left out. May be changed in future!
/// \tparam T type of atomic value
template <typename T>
class IAtomic
{
  public:
    virtual ~IAtomic() = default;

    virtual auto fetch_add(T, std::memory_order) noexcept -> T = 0;
    virtual auto fetch_sub(T, std::memory_order) noexcept -> T = 0;
    virtual bool compare_exchange_strong(T&, T, std::memory_order) noexcept = 0;
    virtual bool compare_exchange_strong(T&, T, std::memory_order, std::memory_order) noexcept = 0;
    virtual bool compare_exchange_weak(T&, T, std::memory_order) noexcept = 0;
    virtual bool compare_exchange_weak(T&, T, std::memory_order, std::memory_order) noexcept = 0;
    virtual void store(T, std::memory_order) noexcept = 0;
    virtual auto load(std::memory_order) noexcept -> T = 0;

  protected:
    // Make all special member functions protected to prevent them ever being explicitly called (which can lead to
    // slicing errors) but can be called by child classes (so that they can automatically generate special member
    // functions)
    IAtomic() noexcept = default;
    IAtomic(const IAtomic&) noexcept = default;
    IAtomic& operator=(const IAtomic&) noexcept = default;
    IAtomic(IAtomic&&) noexcept = default;
    IAtomic& operator=(IAtomic&&) noexcept = default;
};

}  // namespace score::memory::shared

#endif  // BASELIBS_SCORE_MEMORY_SHARED_MOCK_IATOMIC_H
