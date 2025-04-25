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
#ifndef BASELIBS_SCORE_MEMORY_SHARED_MOCK_ATOMICMOCK_H
#define BASELIBS_SCORE_MEMORY_SHARED_MOCK_ATOMICMOCK_H

#include "score/memory/shared/i_atomic.h"

#include "gmock/gmock.h"

#include <atomic>

namespace score::memory::shared
{

template <typename T>
class AtomicMock final : public IAtomic<T>
{
  public:
    MOCK_METHOD(T, fetch_add, (T, std::memory_order), (noexcept, override));
    MOCK_METHOD(T, fetch_sub, (T, std::memory_order), (noexcept, override));
    MOCK_METHOD(bool, compare_exchange_strong, (T&, T, std::memory_order, std::memory_order), (noexcept, override));
    MOCK_METHOD(bool, compare_exchange_strong, (T&, T, std::memory_order), (noexcept, override));
    MOCK_METHOD(bool, compare_exchange_weak, (T&, T, std::memory_order, std::memory_order), (noexcept, override));
    MOCK_METHOD(bool, compare_exchange_weak, (T&, T, std::memory_order), (noexcept, override));
    MOCK_METHOD(void, store, (T, std::memory_order), (noexcept, override));
    MOCK_METHOD(T, load, (std::memory_order), (noexcept, override));
};

}  // namespace score::memory::shared

#endif  // BASELIBS_SCORE_MEMORY_SHARED_MOCK_ATOMICMOCK_H
