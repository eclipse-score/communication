/*********************************************************************************
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
 *******************************************************************************/

#include "score/mw/service/multi_instance_holder.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <future>

namespace score::mw::service::test
{
namespace
{

TEST(MultiInstanceHolder, CanVisitFoundInstances)
{
    // Given a MultiInstanceHolder containing two found elements
    MultiInstanceHolder<std::int32_t> unit{};
    unit.FoundInstance(std::make_unique<std::int32_t>(41));
    unit.FoundInstance(std::make_unique<std::int32_t>(42));

    // When visiting all found instances
    bool found_meaning_of_life{false};
    unit.Visit([&found_meaning_of_life](const auto& item) noexcept {
        if (item == 42)
        {
            found_meaning_of_life = true;
        }
    });

    // Then our instance is found
    EXPECT_TRUE(found_meaning_of_life);
}

TEST(MultiInstanceHolder, CanVisitWithNothingFound)
{
    // Given a MultiInstanceHolder containing no found elements
    MultiInstanceHolder<std::int32_t> unit{};

    // When visiting all found instances
    bool found_meaning_of_life{false};
    unit.Visit([&found_meaning_of_life](const auto& item) noexcept {
        if (item == 42)
        {
            found_meaning_of_life = true;
        }
    });

    // Then our instance is _not_ found
    EXPECT_FALSE(found_meaning_of_life);
}

TEST(MultiInstanceHolder, CanVisitFoundInstancesThreadSafe)
{
    // Given a MultiInstanceHolder where in one thread elements are added
    MultiInstanceHolder<std::size_t> unit{};
    auto producer = std::async(std::launch::async, [&unit]() noexcept {
        for (std::size_t counter{0U}; counter < 100U; counter++)
        {
            unit.FoundInstance(std::make_unique<std::size_t>(counter));
        }
    });

    // When in another thread elements are visited
    bool found_meaning_of_life{false};
    auto consumer = std::async(std::launch::async, [&unit, &found_meaning_of_life]() {
        while (!found_meaning_of_life)
        {
            unit.Visit([&found_meaning_of_life](const auto& item) noexcept {
                if (item == 42U)
                {
                    found_meaning_of_life = true;
                }
            });
        }
    });

    consumer.get();

    // Then our instance is found and no thread sanitizer issue appears.
    EXPECT_TRUE(found_meaning_of_life);

    producer.get();
}

TEST(MultiInstanceHolder, CanInsertDifferentInstanceTypesViaFoundInstance)
{
    class Base
    {
      public:
        virtual ~Base() = default;
        virtual std::int32_t GetValue() const noexcept = 0;
    };

    class Derived final : public Base
    {
      public:
        constexpr explicit Derived(const std::int32_t value) noexcept : value_{value} {}
        std::int32_t GetValue() const noexcept override
        {
            return value_;
        }

      private:
        std::int32_t value_;
    };

    class OtherDerived final : public Base
    {
      public:
        constexpr explicit OtherDerived(const std::int32_t value) noexcept : value_{value} {}
        std::int32_t GetValue() const noexcept override
        {
            return value_;
        }

      private:
        std::int32_t value_;
    };

    // Given a MultiInstanceHolder finding two different instance types
    MultiInstanceHolder<Base> unit{};
    unit.FoundInstance(std::make_unique<Derived>(41));
    unit.FoundInstance(std::make_unique<OtherDerived>(42));

    // When visiting all emplaced instances
    bool found_derived{false};
    bool found_other_derived{false};
    unit.Visit([&](const Base& instance) noexcept {
        if (instance.GetValue() == 42)
        {
            found_other_derived = true;
        }
        if (instance.GetValue() == 41)
        {
            found_derived = true;
        }
    });

    // Then our instances must have gotten visited
    EXPECT_TRUE(found_other_derived);
    EXPECT_TRUE(found_derived);
}

TEST(MultiInstanceHolder, WaitUntilInstancesGotFoundFailsUponRequestStop)
{
    score::cpp::stop_source stop_source{};

    // Given a MultiInstanceHolder without any found element
    MultiInstanceHolder<std::int32_t> unit{};

    // When waiting for a certain instance to be found
    auto future = std::async(std::launch::async, [&unit, &stop_source] {
        return unit.WaitUntilInstancesGotFound(
            [](const auto& found_instances) noexcept {
                return std::any_of(cbegin(found_instances), cend(found_instances), [](const auto& instance) noexcept {
                    return *instance == 42;
                });
            },
            std::chrono::hours{24},
            stop_source.get_token());
    });

    // Then WaitUntilInstancesGotFound() must actually wait
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    ASSERT_EQ(std::future_status::timeout, future.wait_for(std::chrono::milliseconds{0}));

    // When requesting stop at the stop_source
    ASSERT_TRUE(stop_source.request_stop());

    // Then WaitUntilInstancesGotFound() must abort and fail
    EXPECT_FALSE(future.get());
}

TEST(MultiInstanceHolder, WaitUntilInstancesGotFoundFailsDueToStopRequested)
{
    score::cpp::stop_source stop_source;

    // Given a MultiInstanceHolder without any found element
    MultiInstanceHolder<std::int32_t> unit{};

    // When waiting for a certain instance to be found together with a stop_source where stop got already requested
    ASSERT_TRUE(stop_source.request_stop());

    // Then WaitUntilInstancesGotFound() must fail immediately
    EXPECT_FALSE(unit.WaitUntilInstancesGotFound(
        [](const auto& found_instances) noexcept {
            return std::any_of(cbegin(found_instances), cend(found_instances), [](const auto& instance) noexcept {
                return *instance == 42;
            });
        },
        std::chrono::hours{24},
        stop_source.get_token()));
}

TEST(MultiInstanceHolder, WaitUntilInstancesGotFoundTimesOutDueToNoFoundInstances)
{
    // Given a MultiInstanceHolder without any found element
    MultiInstanceHolder<std::int32_t> unit{};

    // When waiting for a certain instance to be found, then it must result in a timeout
    EXPECT_FALSE(unit.WaitUntilInstancesGotFound(
        [](const auto& found_instances) noexcept {
            return std::any_of(cbegin(found_instances), cend(found_instances), [](const auto& instance) noexcept {
                return *instance == 42;
            });
        },
        std::chrono::milliseconds{1},
        score::cpp::stop_token{}));
}

TEST(MultiInstanceHolder, WaitUntilInstancesGotFoundTimesOutDueToUnexpectedFoundOnes)
{
    // Given a MultiInstanceHolder containing two found elements
    MultiInstanceHolder<std::int32_t> unit{};
    unit.FoundInstance(std::make_unique<std::int32_t>(41));
    unit.FoundInstance(std::make_unique<std::int32_t>(43));

    // When waiting for another instance to be found, then it must result in a timeout
    EXPECT_FALSE(unit.WaitUntilInstancesGotFound(
        [](const auto& found_instances) noexcept {
            return std::any_of(cbegin(found_instances), cend(found_instances), [](const auto& instance) noexcept {
                return *instance == 42;
            });
        },
        std::chrono::milliseconds{1},
        score::cpp::stop_token{}));
}

TEST(MultiInstanceHolder, WaitUntilInstancesGotFoundSucceedsUponFoundInstance)
{
    // Given a MultiInstanceHolder containing two found elements
    MultiInstanceHolder<std::int32_t> unit{};
    unit.FoundInstance(std::make_unique<std::int32_t>(41));
    unit.FoundInstance(std::make_unique<std::int32_t>(43));

    // When waiting for another instance to be found
    auto future = std::async(std::launch::async, [&unit] {
        return unit.WaitUntilInstancesGotFound(
            [](const auto& found_instances) noexcept {
                return std::any_of(cbegin(found_instances), cend(found_instances), [](const auto& instance) noexcept {
                    return *instance == 42;
                });
            },
            std::chrono::hours{24},
            score::cpp::stop_token{});
    });

    // Then WaitUntilInstancesGotFound() must actually wait
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    ASSERT_EQ(std::future_status::timeout, future.wait_for(std::chrono::milliseconds{0}));

    // When registering the expected instance
    unit.FoundInstance(std::make_unique<std::int32_t>(42));

    // Then WaitUntilInstancesGotFound() must finish and succeed
    EXPECT_TRUE(future.get());
}

TEST(MultiInstanceHolder, WaitUntilInstancesGotFoundSucceedsImmediately)
{
    // Given a MultiInstanceHolder already containing the three expected instances
    MultiInstanceHolder<std::int32_t> unit{};
    unit.FoundInstance(std::make_unique<std::int32_t>(41));
    unit.FoundInstance(std::make_unique<std::int32_t>(42));
    unit.FoundInstance(std::make_unique<std::int32_t>(43));

    // When waiting for a certain instance to be found, then it must succeed immediately
    EXPECT_TRUE(unit.WaitUntilInstancesGotFound(
        [](const auto& found_instances) noexcept {
            return std::any_of(cbegin(found_instances), cend(found_instances), [](const auto& instance) noexcept {
                return *instance == 42;
            });
        },
        std::chrono::hours{24},
        score::cpp::stop_token{}));

    // When waiting for multiple certain instance to be found, then it must also succeed immediately
    EXPECT_TRUE(unit.WaitUntilInstancesGotFound(
        [](const auto& found_instances) noexcept {
            return (std::any_of(cbegin(found_instances),
                                cend(found_instances),
                                [](const auto& instance) noexcept {
                                    return *instance == 41;
                                }) &&
                    std::any_of(cbegin(found_instances), cend(found_instances), [](const auto& instance) noexcept {
                        return *instance == 43;
                    }));
            ;
        },
        std::chrono::hours{24},
        score::cpp::stop_token{}));
}

}  // namespace
}  // namespace score::mw::service::test
