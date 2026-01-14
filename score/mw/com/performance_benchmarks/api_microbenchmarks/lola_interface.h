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
#ifndef SCORE_MW_COM_PERFORMANCE_BENCHMARKS_MICRO_BENCHMARK_LOLA_INTERFACE_H
#define SCORE_MW_COM_PERFORMANCE_BENCHMARKS_MICRO_BENCHMARK_LOLA_INTERFACE_H

#include "score/mw/com/types.h"

#include <array>
#include <cstdint>

namespace score::mw::com::test
{
namespace
{
using byte = std::uint8_t;
constexpr std::size_t B = 1;
constexpr std::size_t KB = 1024 * B;
constexpr std::size_t MB = KB * KB;
}  // namespace

using DataType = std::array<byte, 5 * MB>;

template <typename T>
struct TestInterface : public T::Base
{
    using T::Base::Base;
    typename T::template Event<DataType> test_event{*this, "test_event"};
};

using TestDataProxy = score::mw::com::AsProxy<TestInterface>;
using TestDataSkeleton = score::mw::com::AsSkeleton<TestInterface>;

}  // namespace score::mw::com::test

#endif  // SCORE_MW_COM_PERFORMANCE_BENCHMARKS_MICRO_BENCHMARK_LOLA_INTERFACE_H
