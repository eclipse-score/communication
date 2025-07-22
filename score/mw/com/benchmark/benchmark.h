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
#ifndef SCORE_IPC_BRIDGE_BENCHMARK_H
#define SCORE_IPC_BRIDGE_BENCHMARK_H

#include "score/mw/com/types.h"

namespace score::mw::com
{

constexpr std::size_t kSampleSize = 8192;
constexpr std::uint32_t kIterations = 10000000 / 100;
constexpr std::size_t kSubscribersA = 2;
constexpr std::size_t kSubscribersB = 2;
constexpr std::size_t kSubscribersTotal = kSubscribersA + kSubscribersB;
constexpr std::size_t kThreadsMultiTotal = kSubscribersTotal + (kSubscribersA != 0 ? 1 : 0) + (kSubscribersB != 0 ? 1 : 0);

struct DummyBenchmarkData
{
    DummyBenchmarkData() = default;

    DummyBenchmarkData(DummyBenchmarkData&&) = default;

    DummyBenchmarkData(const DummyBenchmarkData&) = default;

    DummyBenchmarkData& operator=(DummyBenchmarkData&&) = default;

    DummyBenchmarkData& operator=(const DummyBenchmarkData&) = default;

    std::array<std::uint32_t, kSampleSize / sizeof(std::uint32_t)> dummy_data;

};

template <typename Trait>
class IpcBridgeInterface : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Event<DummyBenchmarkData> dummy_benchmark_data_{*this, "dummy_data_arrived"};
};

using BenchmarkProxy = AsProxy<IpcBridgeInterface>;
using BenchmarkSkeleton = AsSkeleton<IpcBridgeInterface>;

}  // namespace score::mw::com

#endif  // SCORE_IPC_BRIDGE_BENCHMARK_H
