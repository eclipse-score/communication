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

#include "score/mw/com/impl/bindings/lola/consumer_event_data_control_local_view.h"
#include "score/mw/com/impl/bindings/lola/event_data_control.h"
#include "score/mw/com/impl/bindings/lola/test/transaction_log_test_resources.h"
#include "score/mw/com/impl/bindings/lola/test_doubles/fake_memory_resource.h"

#include <gtest/gtest.h>
#include <cstddef>
#include <memory>
#include <type_traits>

namespace score::mw::com::impl::lola
{

class TransactionLogRegistrationGuardTestAttorney
{
  public:
    TransactionLogRegistrationGuardTestAttorney(TransactionLogRegistrationGuard& transaction_log_registration_guard)
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

// While we generally try to only test the public API, checking whether the TransactionLogLocalView is correctly
// injected and cleared in the ConsumerEventDataControlLocalView during construction and destruction of the
// TransactionLogRegistrationGuard is convoluted and error prone if only checking via the public API. It would require
// calling functions on ConsumerEventDataControlLocalView which access the TransactionLog and ensuring that we take the
// correct path to actually access the Transactionlog. This requires setting up a ProviderEventDataControlLocalView,
// ensuring that we set up slots correctly and then using a death test to ensure that we crash when accessing the
// TransactionLog after destruction of the guard. We then have no way of ensuring that we crash due to the
// TransactionLog not being injected rather than anything else. For these reasons, it's simply to directly check that we
// inject / clear the TransactionLog.
class ConsumerEventDataControlLocalViewTestAttorney
{
  public:
    ConsumerEventDataControlLocalViewTestAttorney(
        ConsumerEventDataControlLocalView<>& consumer_event_data_control_local_view)
        : consumer_event_data_control_local_view_(consumer_event_data_control_local_view)
    {
    }

    std::optional<TransactionLogLocalView>& GetTransactionLogLocalView()
    {
        return consumer_event_data_control_local_view_.transaction_log_local_view_;
    }

  private:
    ConsumerEventDataControlLocalView<>& consumer_event_data_control_local_view_;
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
            transaction_log_set_.RegisterProxyElement(kDummyTransactionLogId, consumer_event_data_control_local_view_);
        SCORE_LANGUAGE_FUTURECPP_ASSERT(transaction_log_registration_guard_result.has_value());
        transaction_log_registration_guard_.emplace(std::move(transaction_log_registration_guard_result).value());
        return *this;
    }

    TransactionLog& GetRegisteredTransactionLog()
    {
        return transaction_log_set_.GetTransactionLog(transaction_log_registration_guard_->GetTransactionLogIndex());
    }

    FakeMemoryResource memory_{};
    TransactionLogSet transaction_log_set_{kMaxSlots, kMaxSubscribers, memory_};
    std::optional<TransactionLogRegistrationGuard> transaction_log_registration_guard_{};

    EventDataControl event_data_control_{kMaxSlots, memory_};
    ConsumerEventDataControlLocalView<> consumer_event_data_control_local_view_{event_data_control_};
};

TEST_F(TransactionLogRegistrationGuardFixture, TransactionLogRegistrationGuardUsesScopeExit)
{
    GivenATransactionLogRegistrationGuard();

    TransactionLogRegistrationGuardTestAttorney attorney{transaction_log_registration_guard_.value()};
    using DestructionHandlerType = std::remove_reference_t<decltype(attorney.GetDestructionOperation())>;

    // Expecting that underlying type of the destruction handler is utils::ScopeExit. If this is
    // the case, then we only add basic tests here that UnregisterOnServiceMethodSubscribedHandler is called on
    // destruction of the guard and that the scope of the guard is correctly handled. The more complex tests about
    // testing whether the handler is called when move constructing / move assigning the guard is handled in the
    // tests for ScopeExit.
    static_assert(std::is_same_v<DestructionHandlerType, utils::ScopeExit<score::cpp::callback<void()>>>);
}

TEST_F(TransactionLogRegistrationGuardFixture,
       TransactionLogRegistrationGuardInjectsTransactionLogLocalViewOnConstruction)
{
    // When creating a TransactionLogRegistrationGuard
    GivenATransactionLogRegistrationGuard();

    // Then a TransactionLogLocalView pointing to the TransactionLog that was just registered should have been injected
    // into the ConsumerEventDataControlLocalView.
    ConsumerEventDataControlLocalViewTestAttorney consumer_event_data_control_attorney{
        consumer_event_data_control_local_view_};
    EXPECT_TRUE(consumer_event_data_control_attorney.GetTransactionLogLocalView().has_value());
}

TEST_F(TransactionLogRegistrationGuardFixture, CreatingTransactionLogRegistrationGuardDoesNotCallUnregister)
{
    // When creating a TransactionLogRegistrationGuard
    GivenATransactionLogRegistrationGuard();

    // Then Unregister should not have been called on the underlying TransactionLog
    // We evaluate this by trying to get the TransactionLog, which would terminate if it was already unregistered.
    [[maybe_unused]] TransactionLog& transaction_log =
        transaction_log_set_.GetTransactionLog(transaction_log_registration_guard_->GetTransactionLogIndex());
}

TEST_F(TransactionLogRegistrationGuardFixture, DestroyingTransactionLogRegistrationGuardCallsUnregister)
{
    GivenATransactionLogRegistrationGuard();

    // When destroying the TransactionLogRegistrationGuard
    transaction_log_registration_guard_.reset();

    // Then Unregister should have been called on the underlying TransactionLog
    // We evaluate this by trying to get the TransactionLog, which should terminate if it was already unregistered.
    EXPECT_DEATH(score::cpp::ignore = transaction_log_set_.GetTransactionLog(
                     transaction_log_registration_guard_->GetTransactionLogIndex()),
                 ".*");
}

TEST_F(TransactionLogRegistrationGuardFixture, DestroyingTransactionLogRegistrationGuardClearsTransactionLogLocalView)
{
    GivenATransactionLogRegistrationGuard();

    // When destroying the TransactionLogRegistrationGuard
    ConsumerEventDataControlLocalViewTestAttorney consumer_event_data_control_attorney{
        consumer_event_data_control_local_view_};
    EXPECT_TRUE(consumer_event_data_control_attorney.GetTransactionLogLocalView().has_value());
    transaction_log_registration_guard_.reset();

    // Then a TransactionLogLocalView pointing to the TransactionLog that was just registered should have been cleared
    // from the ConsumerEventDataControlLocalView.
    EXPECT_FALSE(consumer_event_data_control_attorney.GetTransactionLogLocalView().has_value());
}

}  // namespace
}  // namespace score::mw::com::impl::lola
