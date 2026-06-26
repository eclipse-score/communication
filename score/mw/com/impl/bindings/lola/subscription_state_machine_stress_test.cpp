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
#include "score/mw/com/impl/bindings/lola/subscription_state_machine.h"
#include "score/mw/com/impl/bindings/lola/subscription_state_machine_states.h"
#include "score/mw/com/impl/bindings/lola/test/proxy_event_test_resources.h"

#include <gtest/gtest.h>

#include <atomic>
#include <future>

namespace score::mw::com::impl::lola
{
namespace
{

using ::testing::Return;

const TransactionLogId kDummyTransactionLogId{10U};

class StateMachineThreadingFixture : public LolaProxyEventResources
{
  protected:
    StateMachineThreadingFixture()
        : LolaProxyEventResources(),
          consumer_event_data_control_local_view_{proxy_->GetConsumerEventDataControlLocalView(element_fq_id_)},
          state_machine_{proxy_->GetQualityType(),
                         element_fq_id_,
                         kDummyPid,
                         consumer_event_data_control_local_view_,
                         proxy_->GetEventSubscriptionControl(element_fq_id_),
                         proxy_->GetTransactionLogSet(element_fq_id_),
                         kDummyTransactionLogId}
    {
    }

    void SetUp() override
    {
        ON_CALL(runtime_mock_.runtime_mock_, GetBindingRuntime(BindingType::kLoLa))
            .WillByDefault(Return(&binding_runtime_));
    }

    void TearDown() override
    {
        state_machine_.UnsubscribeEvent();
        ProxyMockedMemoryFixture::TearDown();
    }

    void EnterSubscribed(const std::size_t max_samples) noexcept
    {
        const auto result = state_machine_.SubscribeEvent(max_samples);
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(state_machine_.GetCurrentState(), SubscriptionStateMachineState::SUBSCRIBED_STATE);
    }

    ConsumerEventDataControlLocalView<> consumer_event_data_control_local_view_;
    SubscriptionStateMachine state_machine_;
};

// Regression test for a data race on SubscriptionStateMachine::current_state_idx_.
//
// GetSlotCollectorLockFree() calls GetCurrentEventState() which reads current_state_idx_
// WITHOUT holding state_mutex_. Concurrently, StopOfferEvent() and ReOfferEvent() call
// TransitionToState() which writes current_state_idx_ while holding state_mutex_.
//
// The two sides are called from different threads in practice:
//   - User thread: GetNumNewSamplesAvailable() -> GetSlotCollectorLockFree() (read, no lock)
//   - Middleware notification thread: NotifyServiceInstanceChangedAvailability()
//       -> StopOfferEvent() / ReOfferEvent() (write, under state_mutex_)
//
// This test is intended to be run under ThreadSanitizer (TSan) which will detect the race.
TEST_F(StateMachineThreadingFixture, StopOfferEventAndGetSlotCollectorLockFreeRaceOnCurrentStateIdx)
{
    // Given the state machine is in the SUBSCRIBED state
    EnterSubscribed(max_num_slots_);

    constexpr std::size_t kIterations{50000U};
    std::atomic<bool> keep_reading{true};

    // Thread A: simulates the user thread calling GetNumNewSamplesAvailable(), which dispatches to
    // GetSlotCollectorLockFree() and reads current_state_idx_ WITHOUT holding state_mutex_.
    auto reader = std::async(std::launch::async, [&]() {
        while (keep_reading.load(std::memory_order_relaxed))
        {
            std::ignore = state_machine_.GetSlotCollectorLockFree().has_value();
        }
    });

    // Thread B (this thread): simulates the middleware notification thread calling
    // NotifyServiceInstanceChangedAvailability(), which alternates between StopOfferEvent()
    // and ReOfferEvent(). Both write current_state_idx_ under state_mutex_, racing with Thread A.
    for (std::size_t i = 0U; i < kIterations; ++i)
    {
        state_machine_.StopOfferEvent();         // SUBSCRIBED -> PENDING: writes current_state_idx_
        state_machine_.ReOfferEvent(kDummyPid);  // PENDING -> SUBSCRIBED: writes current_state_idx_
    }

    keep_reading.store(false, std::memory_order_relaxed);
    reader.wait();
    // TearDown calls UnsubscribeEvent() to clean up from the final SUBSCRIBED state.
}

}  // namespace
}  // namespace score::mw::com::impl::lola
