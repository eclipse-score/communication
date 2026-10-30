/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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
#include "score/mw/com/test/common_test_resources/shared_memory_object_creator.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace score::mw::com::test
{
namespace
{

struct SharedCounter
{
    std::atomic<std::int32_t> value{0};
};

TEST(SharedMemoryObjectCreatorTest, ConcurrentCreateOrOpenAlwaysSucceedsAndSharesOneObject)
{
    constexpr std::size_t kNumParticipants{8U};
    constexpr std::size_t kNumIterations{2000U};

    for (std::size_t iteration = 0U; iteration < kNumIterations; ++iteration)
    {
        const std::string shm_name =
            "shared_memory_object_creator_test_" + std::to_string(::getpid()) + "_" + std::to_string(iteration);
        std::atomic<bool> start{false};
        std::atomic<std::size_t> failures{0U};
        std::vector<std::unique_ptr<SharedMemoryObjectCreator<SharedCounter>>> creators(kNumParticipants);

        std::vector<std::thread> threads;
        for (std::size_t i = 0U; i < kNumParticipants; ++i)
        {
            threads.emplace_back([&, i]() {
                while (!start.load())
                {
                }
                auto result = SharedMemoryObjectCreator<SharedCounter>::CreateOrOpenObject(shm_name);
                if (!result.has_value())
                {
                    ++failures;
                    return;
                }
                result.value().GetObject().value++;
                creators[i] = std::make_unique<SharedMemoryObjectCreator<SharedCounter>>(std::move(result).value());
            });
        }
        start.store(true);
        for (auto& thread : threads)
        {
            thread.join();
        }

        ASSERT_EQ(failures.load(), 0U) << "iteration " << iteration;
        for (auto& creator : creators)
        {
            ASSERT_NE(creator, nullptr);
            EXPECT_EQ(creator->GetObject().value.load(), static_cast<std::int32_t>(kNumParticipants));
        }
        for (auto& creator : creators)
        {
            creator->CleanUp();
        }
    }
}

}  // namespace
}  // namespace score::mw::com::test
