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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_ATOMICINDIRECTOR_H
#define BASELIBS_SCORE_MEMORY_SHARED_ATOMICINDIRECTOR_H

#include "score/memory/shared/i_atomic.h"

#include <atomic>
#include <type_traits>

namespace score::memory::shared
{

/**
 * \brief Helper class to allow mocking of atomic operations via static dispatch.
 *
 * We use atomic operations such as fetch_add, compare_exchange_weak/strong etc. in various points in our code
 * base which produce different code paths based on the result of these operations. It is very difficult to test these
 * code paths as they are conditional on whether the atomic value has updated at a very specific time which is difficult
 * to achieve reliably in tests. Therefore, we want to be able to mock these calls in our tests.
 *
 * Since the atomic operations are often used in time-critical parts of the system, we don't want to use the common
 * approach of calling operations on a virtual base class and choosing the real or mock implementation at runtime as the
 * dynamic dispatch can add additional runtime overhead. Instead, we use a static dispatch approach so that the correct
 * implementation (real or mock) is chosen at compile time. When the real implementation is chosen, there is no
 * additional overhead as the abstraction is optimized away by the compiler.
 *
 * The downside of this approach is that classes which use this mechanism must be templated with the real or mock
 * implementation class (i.e. AtomicIndirectorReal or AtomicIndirectorMock). To use this class, an existing class should
 * be modified as follows (see platform/aas/mw/com/impl/bindings/lola/event_data_control.h for an example):
 *
 *      template <template <class> class AtomicIndirectorType = memory::shared::AtomicIndirectorReal>
 *      class ExistingClass
 *
 * Calls to atomic operations within ExistingClass should be called as follows (assuming the atomic type is int):
 *
 *      AtomicIndirectorType<int>::compare_exchange_weak(atomic_value, expected, desired);
 *
 * To prevent having to modify calling code, you can change the existing class name to ExistingClassImpl, add it to a
 * detail namespace and add an alias:
 *
 *      using ExistingClass = detail::ExistingClassImpl<>.
 *
 * Then in test code, you can instantiate it using detail::ExistingClassImpl<AtomicIndirectorMock> in order to access
 * the mock methods. See atomic_indirector_test.cpp for examples of mock usage.
 */
template <typename T>
class AtomicIndirectorReal
{
  public:
    // is_always_lock_free is a compile-time constant so is not suitable for mocking. We simply provide it here in case
    // its required in production code.
    static constexpr bool is_always_lock_free = std::atomic<T>::is_always_lock_free;

    template <typename... Args>
    static T fetch_add(std::atomic<T>& value, Args&&... args) noexcept
    {
        // Suppress "AUTOSAR C++14 M5-0-3" rule findings. This rule states: "A cvalue expression shall
        // not be implicitly converted to a different underlying type"
        // Rationale: There is no implicit conversion. std::atomic<T>::fetch_add returns type T, which is what we return
        // from this function.
        // coverity[autosar_cpp14_m5_0_3_violation : FALSE]
        return value.fetch_add(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static T fetch_sub(std::atomic<T>& value, Args&&... args) noexcept
    {
        // Suppress "AUTOSAR C++14 M5-0-3" rule findings. This rule states: "A cvalue expression shall
        // not be implicitly converted to a different underlying type"
        // Rationale: There is no implicit conversion. std::atomic<T>::fetch_sub returns type T, which is what we return
        // from this function.
        // coverity[autosar_cpp14_m5_0_3_violation : FALSE]
        return value.fetch_sub(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static bool compare_exchange_strong(std::atomic<T>& value, Args&&... args) noexcept
    {
        return value.compare_exchange_strong(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static bool compare_exchange_weak(std::atomic<T>& value, Args&&... args) noexcept
    {
        return value.compare_exchange_weak(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void store(std::atomic<T>& value, Args&&... args) noexcept
    {
        value.store(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static auto load(const std::atomic<T>& value, Args&&... args) noexcept -> auto
    {
        return value.load(std::forward<Args>(args)...);
    }
};

template <typename T>
class AtomicIndirectorMock
{
  public:
    static void SetMockObject(IAtomic<T>* const mock_object)
    {
        mock_object_ = mock_object;
    }
    static IAtomic<T>* GetMockObject()
    {
        return mock_object_;
    }

    template <typename... Args>
    static auto fetch_add(std::atomic<T>& /*value*/, Args&&... args) noexcept -> T
    {
        return mock_object_->fetch_add(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static T fetch_sub(std::atomic<T>& /*value*/, Args&&... args) noexcept
    {
        return mock_object_->fetch_sub(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static bool compare_exchange_strong(std::atomic<T>& /*value*/, Args&&... args) noexcept
    {
        return mock_object_->compare_exchange_strong(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static bool compare_exchange_weak(std::atomic<T>& /*value*/, Args&&... args) noexcept
    {
        return mock_object_->compare_exchange_weak(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static void store(std::atomic<T>& /*value*/, Args&&... args) noexcept
    {
        mock_object_->store(std::forward<Args>(args)...);
    }

    template <typename... Args>
    static auto load(const std::atomic<T>& /*value*/, Args&&... args) noexcept -> T
    {
        return mock_object_->load(std::forward<Args>(args)...);
    }

  private:
    static IAtomic<T>* mock_object_;
};

template <typename T>
// Suppress "AUTOSAR C++14 A3-1-1" rule finding: " It shall be possible to include any header file in multiple
// translation units without violating the One Definition Rule.".
// The definition of the static data member 'mock_object_' in this header file is necessary due to template
// instantiation requirements. Each instantiation of 'AtomicIndirectorMock<T>' needs its own static instance of
// 'mock_object_'. This is permitted under the One Definition Rule (ODR) exception for templates. The AUTOSAR C++14
// A3-1-1 guideline is not violated in this context as this is a template class. All instantiations share the same
// definition provided here.
// coverity[autosar_cpp14_a3_1_1_violation]
IAtomic<T>* AtomicIndirectorMock<T>::mock_object_{nullptr};

}  // namespace score::memory::shared

#endif  // BASELIBS_SCORE_MEMORY_SHARED_ATOMICINDIRECTOR_H
