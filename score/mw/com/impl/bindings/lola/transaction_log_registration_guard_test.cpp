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
#include "score/mw/com/impl/bindings/lola/transaction_log_registration_guard.h"

#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/test/transaction_log_test_resources.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include <gtest/gtest.h>
#include <cstddef>

namespace score::mw::com::impl::lola
{
namespace
{

constexpr std::size_t kMaxSlots{5U};
constexpr std::size_t kMaxSubscribers{5U};
const TransactionLogId kDummyTransactionLogId{10U};

using TransactionLogRegistrationGuardFixture = TransactionLogSetHelperFixture;

TEST_F(TransactionLogRegistrationGuardFixture, CreatingGuardWithAvailableSlotsWillReturnGuard)
{
    FakeMemoryResource memory{};
    EventDataControl event_data_control{kMaxSlots, memory.getMemoryResourceProxy(), kMaxSubscribers};

    auto unit = TransactionLogRegistrationGuard::Create(event_data_control, kDummyTransactionLogId);
    EXPECT_TRUE(unit.has_value());
}

TEST_F(TransactionLogRegistrationGuardFixture, CreatingGuardWithoutAvailableSlotsWillReturnError)
{
    constexpr std::size_t max_subscribers{1U};

    FakeMemoryResource memory{};
    EventDataControl event_data_control{kMaxSlots, memory.getMemoryResourceProxy(), max_subscribers};

    auto unit = TransactionLogRegistrationGuard::Create(event_data_control, kDummyTransactionLogId);
    EXPECT_TRUE(unit.has_value());

    auto unit_2 = TransactionLogRegistrationGuard::Create(event_data_control, kDummyTransactionLogId);
    EXPECT_FALSE(unit_2.has_value());
}

TEST_F(TransactionLogRegistrationGuardFixture, DestroyingGuardWillUnregisterLog)
{
    const bool expect_needs_rollback{false};

    FakeMemoryResource memory{};
    EventDataControl event_data_control{kMaxSlots, memory.getMemoryResourceProxy(), kMaxSubscribers};
    auto& transaction_log_set = event_data_control.GetTransactionLogSet();

    {
        ExpectTransactionLogSetEmpty(transaction_log_set);
        auto unit = TransactionLogRegistrationGuard::Create(event_data_control, kDummyTransactionLogId).value();
        const auto transaction_log_index = unit.GetTransactionLogIndex();
        ExpectProxyTransactionLogExistsAtIndex(
            transaction_log_set, kDummyTransactionLogId, transaction_log_index, expect_needs_rollback);
    }
    ExpectTransactionLogSetEmpty(transaction_log_set);
}

TEST_F(TransactionLogRegistrationGuardFixture, MoveConstructingGuardWillNotUnregisterLog)
{
    const bool expect_needs_rollback{false};

    FakeMemoryResource memory{};
    EventDataControl event_data_control{kMaxSlots, memory.getMemoryResourceProxy(), kMaxSubscribers};
    auto& transaction_log_set = event_data_control.GetTransactionLogSet();

    {
        ExpectTransactionLogSetEmpty(transaction_log_set);
        auto unit = TransactionLogRegistrationGuard::Create(event_data_control, kDummyTransactionLogId).value();
        const auto transaction_log_index = unit.GetTransactionLogIndex();

        TransactionLogRegistrationGuard unit2{std::move(unit)};
        const auto transaction_log_index_2 = unit2.GetTransactionLogIndex();

        EXPECT_EQ(transaction_log_index, transaction_log_index_2);
        ExpectProxyTransactionLogExistsAtIndex(
            transaction_log_set, kDummyTransactionLogId, transaction_log_index, expect_needs_rollback);
    }
    ExpectTransactionLogSetEmpty(transaction_log_set);
}

}  // namespace
}  // namespace score::mw::com::impl::lola
