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
#ifndef SCORE_MW_COM_PERFORMANCE_BENCHMARKS_COMMON_TEST_RESOURCES_SHARED_MEMORY_OBJECT_GUARD_H
#define SCORE_MW_COM_PERFORMANCE_BENCHMARKS_COMMON_TEST_RESOURCES_SHARED_MEMORY_OBJECT_GUARD_H

#include "score/mw/com/performance_benchmarks/common_test_resources/shared_memory_object_creator.h"

namespace score::mw::com::test
{

template <typename T>
class SharedMemoryObjectGuard final
{
  public:
    explicit SharedMemoryObjectGuard(SharedMemoryObjectCreator<T>& shared_memory_object_creator) noexcept
        : shared_memory_object_creator_{shared_memory_object_creator}
    {
    }

    ~SharedMemoryObjectGuard() noexcept
    {
        shared_memory_object_creator_.CleanUp();
    }

    SharedMemoryObjectGuard(const SharedMemoryObjectGuard&) = delete;
    SharedMemoryObjectGuard& operator=(const SharedMemoryObjectGuard&) = delete;
    SharedMemoryObjectGuard(SharedMemoryObjectGuard&& other) noexcept = delete;
    SharedMemoryObjectGuard& operator=(SharedMemoryObjectGuard&& other) noexcept = delete;

  private:
    SharedMemoryObjectCreator<T>& shared_memory_object_creator_;
};

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_PERFORMANCE_BENCHMARKS_COMMON_TEST_RESOURCES_SHARED_MEMORY_OBJECT_GUARD_H
