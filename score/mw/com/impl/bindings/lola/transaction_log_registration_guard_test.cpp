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
#include "score/mw/com/impl/bindings/lola/proxy_event_data_control_local_view.h"
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

class TransactionLogRegistrationGuardFixture : public TransactionLogSetHelperFixture
{
  public:
    TransactionLogRegistrationGuardFixture& GivenAnEventDataControl(
        const SlotIndexType max_slots,
        const LolaEventInstanceDeployment::SubscriberCountType max_subscribers)
    {
        event_data_control_.emplace(max_slots, memory_, max_subscribers);
        proxy_event_data_control_local_.emplace(event_data_control_.value());
        return *this;
    }

    FakeMemoryResource memory_{};
    std::optional<EventDataControl> event_data_control_{};
    std::optional<ProxyEventDataControlLocalView<>> proxy_event_data_control_local_{};
};

TEST_F(TransactionLogRegistrationGuardFixture, CreatingGuardWithAvailableSlotsWillReturnGuard)
{
    GivenAnEventDataControl(kMaxSlots, kMaxSubscribers);
    auto unit =
        TransactionLogRegistrationGuard::Create(proxy_event_data_control_local_.value(), kDummyTransactionLogId);
    EXPECT_TRUE(unit.has_value());
}

TEST_F(TransactionLogRegistrationGuardFixture, CreatingGuardWithoutAvailableSlotsWillReturnError)
{
    constexpr std::size_t max_subscribers{1U};

    GivenAnEventDataControl(kMaxSlots, max_subscribers);
    auto unit =
        TransactionLogRegistrationGuard::Create(proxy_event_data_control_local_.value(), kDummyTransactionLogId);
    EXPECT_TRUE(unit.has_value());

    auto unit_2 =
        TransactionLogRegistrationGuard::Create(proxy_event_data_control_local_.value(), kDummyTransactionLogId);
    EXPECT_FALSE(unit_2.has_value());
}

TEST_F(TransactionLogRegistrationGuardFixture, DestroyingGuardWillUnregisterLog)
{
    const bool expect_needs_rollback{false};

    GivenAnEventDataControl(kMaxSlots, kMaxSubscribers);
    auto& transaction_log_set = proxy_event_data_control_local_->GetTransactionLogSet();

    {
        ExpectTransactionLogSetEmpty(transaction_log_set);
        auto unit =
            TransactionLogRegistrationGuard::Create(proxy_event_data_control_local_.value(), kDummyTransactionLogId)
                .value();
        const auto transaction_log_index = unit.GetTransactionLogIndex();
        ExpectProxyTransactionLogExistsAtIndex(
            transaction_log_set, kDummyTransactionLogId, transaction_log_index, expect_needs_rollback);
    }
    ExpectTransactionLogSetEmpty(transaction_log_set);
}

TEST_F(TransactionLogRegistrationGuardFixture, MoveConstructingGuardWillNotUnregisterLog)
{
    const bool expect_needs_rollback{false};

    GivenAnEventDataControl(kMaxSlots, kMaxSubscribers);
    auto& transaction_log_set = proxy_event_data_control_local_->GetTransactionLogSet();

    {
        ExpectTransactionLogSetEmpty(transaction_log_set);
        auto unit =
            TransactionLogRegistrationGuard::Create(proxy_event_data_control_local_.value(), kDummyTransactionLogId)
                .value();
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
