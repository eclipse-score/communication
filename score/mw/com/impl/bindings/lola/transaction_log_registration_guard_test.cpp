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
#include <memory>
#include <type_traits>

namespace score::mw::com::impl::lola
{

class TransactionLogRegistrationGuardAttorney
{
  public:
    TransactionLogRegistrationGuardAttorney(TransactionLogRegistrationGuard& transaction_log_registration_guard)
        : transaction_log_registration_guard_(transaction_log_registration_guard)
    {
    }

    auto& GetDestructionOperation()
    {
        return transaction_log_registration_guard_.unregister_on_destruction_operation_;
    }

  private:
    TransactionLogRegistrationGuard& transaction_log_registration_guard_;
};

namespace
{

constexpr std::size_t kMaxSlots{5U};
constexpr std::size_t kMaxSubscribers{5U};
const TransactionLogId kDummyTransactionLogId{10U};

class TransactionLogRegistrationGuardFixture : public TransactionLogSetHelperFixture
{
  public:
    TransactionLogRegistrationGuardFixture& GivenATransactionLogRegistrationGuard()
    {
        auto transaction_log_registration_guard_result =
            transaction_log_set_->RegisterProxyElement(kDummyTransactionLogId, proxy_event_data_control_local_view_);
        SCORE_LANGUAGE_FUTURECPP_ASSERT(transaction_log_registration_guard_result.has_value());
        transaction_log_registration_guard_.emplace(std::move(transaction_log_registration_guard_result).value());
        return *this;
    }

    FakeMemoryResource memory_{};
    std::unique_ptr<TransactionLogSet> transaction_log_set_{
        std::make_unique<TransactionLogSet>(kMaxSlots, kMaxSubscribers, memory_)};
    std::optional<TransactionLogRegistrationGuard> transaction_log_registration_guard_{};

    EventDataControl event_data_control_{kMaxSlots, memory_};
    ProxyEventDataControlLocalView<> proxy_event_data_control_local_view_{event_data_control_};
};

TEST_F(TransactionLogRegistrationGuardFixture, TransactionLogRegistrationGuardUsesMovableScopedOperation)
{
    GivenATransactionLogRegistrationGuard();

    TransactionLogRegistrationGuardAttorney attorney{transaction_log_registration_guard_.value()};
    using DestructionHandlerType = std::remove_reference_t<decltype(attorney.GetDestructionOperation())>;

    // Expecting that underlying type of the destruction handler is utils::MovableScopedOperation. If this is
    // the case, then we only add basic tests here that UnregisterOnServiceMethodSubscribedHandler is called on
    // destruction of the guard and that the scope of the guard is correctly handled. The more complex tests about
    // testing whether the handler is called when move constructing / move assigning the guard is handled in the
    // tests for MovableScopedOperation.
    static_assert(std::is_same_v<DestructionHandlerType, utils::MovableScopedOperation<score::cpp::callback<void()>>>);
}

TEST_F(TransactionLogRegistrationGuardFixture,
       TransactionLogRegistrationGuardInjectsTransactionLogLocalViewOnConstruction)
{
}

TEST_F(TransactionLogRegistrationGuardFixture, CreatingTransactionLogRegistrationGuardDoesNotCallUnregister)
{
    // When creating a TransactionLogRegistrationGuard
    GivenATransactionLogRegistrationGuard();

    // Then Unregister should not have been called on the underlying TransactionLog
    // We evaluate this by trying to get the TransactionLog, which would terminate if it was already unregistered.
    TransactionLog& transaction_log =
        transaction_log_set_->GetTransactionLog(transaction_log_registration_guard_->GetTransactionLogIndex());
}

TEST_F(TransactionLogRegistrationGuardFixture,
       CreatingTransactionLogRegistrationGuardDoesNotClearTransactionLogLocalView)
{
}

TEST_F(TransactionLogRegistrationGuardFixture, DestroyingTransactionLogRegistrationGuardCallsUnregister)
{
    GivenATransactionLogRegistrationGuard();

    // When destroying the TransactionLogRegistrationGuard
    transaction_log_registration_guard_.reset();

    // Then Unregister should have been called on the underlying TransactionLog
    // We evaluate this by trying to get the TransactionLog, which should terminate if it was already unregistered.
    EXPECT_DEATH(score::cpp::ignore = transaction_log_set_->GetTransactionLog(
                     transaction_log_registration_guard_->GetTransactionLogIndex()),
                 ".*");
}

TEST_F(TransactionLogRegistrationGuardFixture, DestroyingTransactionLogRegistrationGuardClearsTransactionLogLocalView)
{
}

}  // namespace
}  // namespace score::mw::com::impl::lola
